class LRUCache {
private:
    unordered_map<string, pair<Response, list<string>::iterator>> cache;
    list<string> keylist;
    int capacity;
    //lock

public:
    LRUCache(int cap) { 
        this->capacity = cap; 
    }
    // maintain cache with lru algorithms
    void move_cache(
        unordered_map<string, pair<Response, list<int>::iterator>>::iterator &it, 
        string url, Response response); 
    // check if the url in the cache
    bool search_cache(string url);
    // get response from cache
    Response get_cache(string url); 
    // insert new entry
    void insert_cache(string url, Response response); 
    // update response
    void update_cache(string url, Response response); 
}

void LRUCache::move_cache(
        unordered_map<string, pair<Response, list<int>::iterator>>::iterator &it, 
        string url, Response response) {
    auto old = it->second.second;
    keylist.erase(old);
    keylist.push_front(url);
    cache[url] = make_pair(response, keylist.begin());
}

bool LRUCache::search_cache(string url) {
    auto it = cache.find(url);
    if (it != cache.end()){
        return true;
    }
    return false;
}

Response LRUCache::get_cache(string url) {
    auto it = cache.find(url);
    Response response = it->second.first;
    move_cache(it, url, response);
    return response;
}

void LRUCache::insert_cache(string url, Response response) {
    keylist.push_front(url);
    cache[url] = make_pair(response, keylist.begin());
    if (cache.size() > capacity) {
        string remove_url = keylist.back();
        cache.erase(remove_url);
        kelist.erase(prev(keylist.end()));
    }
} 

void LRUCache::update_cache(string url, Response response) {
    keylist.push_front(url);
    cache[url] = make_pair(response, keylist.begin());
}