/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <mutex>
#include <list>
#include <chrono>


using namespace std::chrono;

/* A threadsafe LRU cache with optional auto-expiration of cache elements.
 * Use std::shared_ptr as data elements for a non-owning cache (probably a good idea in multithread environments).
 * Note that the dirty property overrides item expiration (a non-removable item will never be considered expired). */
template< typename Key, typename Data > class LRUcache final {

private:
    typedef std::pair< Data, steady_clock::time_point > cache_entry;

    std::list<cache_entry> cache;
    std::unordered_map<Key, typename std::list<cache_entry>::iterator > lookup;

    std::unordered_set<Key> blocked_keys;
    std::condition_variable unblocked;

    milliseconds        expiration_time;
    std::uint32_t       capacity;
    std::mutex          mutex;

    std::function<const Key(const Data&)> getKey;
    std::function<bool(const Data&)> dirty;

private:
    bool expired(typename std::list<cache_entry>::iterator it){
        // dirty elements cannot be expired from the cache
        if(dirty(it->first)) return false;
        // an item is considered expired if it has been in the cache for longer than expiration_time
        return expiration_time.count() && (duration_cast<milliseconds>(steady_clock::now() - it->second) > expiration_time);
    }
    void remove(const Key &k){
        cache.erase(lookup[k]);
        lookup.erase(k);
    }

public:
    bool __attribute__((warn_unused_result)) get(const Key& k, Data& d){
        std::unique_lock<std::mutex> locker(mutex);

        while(blocked_keys.count(k))
            unblocked.wait(locker);

        if(lookup.count(k) == 0)
            return false;

        if(expired(lookup[k])){
            remove(k);
            return false;
        }
        cache.splice( cache.begin(), cache, lookup[k] );
        d = lookup[k]->first;
        return true;
    }

    bool __attribute__((warn_unused_result)) add(const Key& k, Data& d){
        std::unique_lock<std::mutex> locker(mutex);

        if(lookup.count(k) > 0){
            if(expired(lookup[k])) remove(k);
            else return false;
        }

        for (auto it = cache.rbegin(); it!=cache.rend() && cache.size() >= capacity; ++it){
            if(!dirty(it->first))
                remove(getKey(it->first));
        }

        cache.push_front(cache_entry(d, steady_clock::now()));
        lookup[k] = cache.begin();

        if(blocked_keys.erase(k))
            unblocked.notify_all();
        return true;
    }

    bool __attribute__((warn_unused_result)) block(const Key &k){
        std::unique_lock<std::mutex> locker(mutex);
        return blocked_keys.insert(k).second;
    }

    void revalidate(const Key& k){
        std::unique_lock<std::mutex> locker(mutex);
        if(lookup.count(k) > 0)
            lookup[k]->second = steady_clock::now();
    }

    void invalidate(const Key& k){
        std::unique_lock<std::mutex> locker(mutex);
        if(lookup.count(k) > 0)
            remove(k);
        if(blocked_keys.erase(k))
            unblocked.notify_all();
    }

public:
    /* Set expiration time to 0 to disable expiration.
     * Capacity limit can be exceeded if the cache contains only non-removable objects.*/
    explicit LRUcache(std::uint64_t expiration_milliseconds,
                      std::uint32_t capacity,
                      std::function<const Key(const Data&)> getKey,
                      std::function<bool (const Data&)> dirty):
         cache(), lookup(), expiration_time(expiration_milliseconds), capacity(capacity), getKey(getKey), dirty(dirty) {};
    ~LRUcache(){};
};

#endif
