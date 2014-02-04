#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_

#include <unordered_map>
#include <mutex>
#include <list>
#include "debug.h"

/* use std::shared_ptr<Data> for a non-owning LRU cache */
template< typename Key, typename Data > class LRUcache final {

private:
    std::uint32_t      capacity;
    std::mutex         mutex;
    std::list < Data > cache;
    std::unordered_map<Key, typename std::list<Data>::iterator> lookup;

    std::function<const Key(Data&)> getKey;

public:
    bool get(const Key& k, Data& d){
        std::lock_guard<std::mutex> locker(mutex);
        if(lookup.count(k) == 0)
            return false;

        cache.splice( cache.begin(), cache, lookup[k] );
        d = *lookup[k];
        return true;
    }

    bool add(const Key& k, Data& d){
      std::lock_guard<std::mutex> locker(mutex);
      if(lookup.count(k) > 0)
          return false;

      if(cache.size() >= capacity){
          const Key last = getKey(cache.back());
          lookup.erase(last);
          cache.pop_back();
      }

      cache.push_front(d);
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
    explicit LRUcache(std::uint32_t capacity, std::function<const Key(Data&)> getKey):
    capacity(capacity), getKey(getKey) {}
    ~LRUcache(){};
};

#endif
