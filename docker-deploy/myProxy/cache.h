//
// Created by 徐颖 on 2/25/20.
//

#ifndef PROXY_LRU_CACHE_H
#define PROXY_LRU_CACHE_H

#include <list>
#include <iterator>
#include <utility>
#include <unordered_map>
#include "response.h"
#include "request.h"
#include <string>
#include <mutex>
mutex cache_lock;
using namespace std;

/**
 * Remember to add mutex to handle multithread thing!!
 */
class LRUCache {
    int cap;
    // key is the key, value is the iterator of list
    unordered_map <string, list<pair<string, Response*>>::iterator> my_map;
    // it->first = key, it->second = value of data.
    list<pair<string, Response*>> my_list;
public:
    explicit LRUCache(int capacity): cap(capacity){
    }
    // Get the value (will always be positive) of the key if the key exists in the cache,
    // otherwise return -1.
    Response* get(string key) {
        cache_lock.lock();
        auto found_it = my_map.find(key);
        cache_lock.unlock();
        if (found_it != my_map.end()) {
            // key exists, get value
            auto it = found_it->second;
            my_list.splice(my_list.begin(), my_list, it);
            return my_list.begin()->second;
        }
        return nullptr;
    }

    // Set or insert the value if the key is not already present
    // When the cache reached its capacity,
    // it should invalidate the least recently used item before inserting a new item
    void put(string key, Response* value) {
        cache_lock.lock();
        auto found_it = my_map.find(key);
        if (found_it != my_map.end()) {
            // exist
            auto it = found_it->second;
            my_list.splice(my_list.begin(), my_list, it);
            it->second = value;

        } else {
            // does not exist
            my_list.emplace_front(key, value);
            my_map[key] = my_list.begin();
            if (my_map.size() > cap) {
                string to_be_delete = my_list.back().first;
                my_list.pop_back();
                my_map.erase(to_be_delete);
            }
        }
        cache_lock.unlock();
    }
};

LRUCache cache(99999);
/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */
#endif //PROXY_LRU_CACHE_H
