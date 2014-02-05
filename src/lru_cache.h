#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_
#include <unordered_map>
#include <mutex>
#include <list>
#include <chrono>

using namespace std::chrono;

/* A threadsafe LRU cache with auto-expiration of cache elements.
 * Use std::shared_ptr as data elements for a non-owning cache (probably a good idea in multithread environments) */

template< typename Key, typename Data > class LRUcache final {

private:
    typedef std::pair< Data, steady_clock::time_point > cache_entry;

    std::list<cache_entry> cache;
    std::unordered_map<Key, typename std::list<cache_entry>::iterator > lookup;

    bool                expiration_refresh;
    milliseconds        expiration_time;
    std::uint32_t       capacity;
    std::mutex          mutex;

    std::function<const Key(Data&)> getKey;


public:
    bool get(const Key& k, Data& d){
        std::lock_guard<std::mutex> locker(mutex);
        if(lookup.count(k) == 0)
            return false;

        if(expiration_time.count() && (duration_cast<milliseconds>(steady_clock::now() - lookup[k]->second) > expiration_time)){
            cache.erase(lookup[k]);
            lookup.erase(k);
            return false;
        }
        if(expiration_refresh)
            lookup[k]->second = steady_clock::now();

        cache.splice( cache.begin(), cache, lookup[k] );
        d = lookup[k]->first;
        return true;
    }

    bool add(const Key& k, Data& d){
      std::lock_guard<std::mutex> locker(mutex);

      if(lookup.count(k) > 0){
          if(expiration_time.count() && (duration_cast<milliseconds>(steady_clock::now() - lookup[k]->second) > expiration_time)){
               cache.erase(lookup[k]);
               lookup.erase(k);
          }
          else return false;
      }

      if(cache.size() >= capacity){
          const Key last = getKey(cache.back().first);
          lookup.erase(last);
          cache.pop_back();
      }

      cache.push_front(cache_entry(d, steady_clock::now()));
      lookup[k] = cache.begin();
      return true;
    }

    void invalidate(const Key& k){
        std::lock_guard<std::mutex> locker(mutex);
        if(lookup.count(k) > 0){
            cache.erase(lookup[k]);
            lookup.erase(k);
        }
    }

public:
    explicit LRUcache(std::function<const Key(Data&)> getKey,
                      std::uint32_t capacity,
                      std::uint64_t expiration_milliseconds,
                      bool          expiration_refresh_on_get):
         expiration_refresh(expiration_refresh_on_get), expiration_time(expiration_milliseconds), capacity(capacity), getKey(getKey) {};
    ~LRUcache(){};
};

#endif
