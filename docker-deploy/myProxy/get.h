
#include "cache.h"
#include "request.h"
#include "response.h"
#include "proxy.h"
#include <iostream>
#include <sys/socket.h>
#include <zconf.h>

using namespace std;

//void Proxy::get_request(){
//    std::map<std::string, std::string> request_header = request->get_header();
//    std::string request_cache_control;
//    std::string request_url = request.get_full_url();
//
//    Response response_cached;
//    std::map<std::string, std::string> response_cached_header;
//    std::string response_cached_cache_control;
//
//    Response response_refetched;
//    std::map<std::string, std::string> response_refetched_header;
//
//    bool response_in_cache = cache.search_cache(request_url);
//    bool expired;
//    bool valid;
//    int status;
//    time_t expired_time;
//    bool can_update(Response response);
//    bool check_expired();
//
//    // in cache
//    if(response_in_cache == true){
//        response_cached = cache.get_cache(request_url);
//        response_cached_header = response_cached.get_header();
//        expired_time = get_expiration_time();
//        // expired
//        if (expired == true) {
//            std::cout << 'in cache, but expired at ' << expired_time << std::endl;
//            // refetching
//            response_refetched = refetching(server_fd, request);
//            // update cache
//            if (can_update(response_refetched) == true) {
//                cache.update_cache(request_url, response_refetched);
//            }
//            // send(client_fd, response_refetched);
//        }
//        else {// if not expired
//            // request cache-control
//            auto req_it = request_header.find("Cache-Control");
//            if (req_it != request_header.end()) {
//                request_cache_control = req_it->second;
//                // no-cahce -> revalidate
//                if(request_cache_control.find("no-cache") != string::npos) {
//                    // revalidate
//                    std::cout << 'in cache, but need to revalidate' << std::endl;
//                    revalidation();
//                    if(valid == true) {
//                        std::cout << 'in cache, revalidation succeed, valid' << std::endl;
//                        // send(client_fd, responsed_cached);
//                    }
//                    else {
//                        std::cout << 'in cache, revalidation failed, refetching' << std::endl;
//                        // refetching
//                        response_refetched = refetching(server_fd, request);
//                        // update
//                        if (can_update(response_refetched) == true) {
//                            cache.update_cache(request_url, response_refetched);
//                        }
//                        // send(client_fd, response_refetched);
//                    }
//
//                }
//                // no-store -> refetching
//                if (request_header.find("no-store")!=request_header.end()){
//                    std::cout << 'in cache, but request saying no-store, refetching' << std::endl;
//                    // refetching
//                    response_refetched = refetching(server_fd, request);
//                    // update
//                    if (can_update(response_refetched)) {
//                        cache.update_cache(request_url, response_refetched);
//                    }
//
//                    // send(client_fd, response_refetched);
//                }
//                // else {
//                //     std::cout << 'in cache, valid' << std::endl;
//                //     // send(client_fd, responsed_cached);
//                // }
//            }
//
//            // response cache-control
//            auto resp_it = response_cached_header.find("Cache-Control");
//            if (resp_it != response_cached_header.end()) {
//                response_cached_cache_control = resp_it->second;
//                if (response_cached_cache_control.find("no-cache") != string::npos ||
//                    response_cached_cache_control.find("must-revalidate") != string::npos ||
//                    response_cached_cache_control.find("proxy-revalidate") != string::npos ) {
//                    std::cout << 'in cache, but need to revalidate' << std::endl;
//                    revalidation();
//                    if (valid == true) {
//                        std::cout << 'in cache, revalidation succeed, valid' << std::endl;
//                        // send(client_fd, responsed_cached);
//                    }
//                    else {
//                        std::cout << 'in cache, revalidation failed, refetching' << std::endl;
//                        // refetching
//                        response_refetched = refetching(server_fd, request);
//                        // update
//                        if (can_update(response_refetched) == true) {
//                            cache.update_cache(request_url, response_refetched);
//                        }
//                    }
//                }
//                // else {
//                //     std::cout << 'in cache, valid' << std::endl;
//                //     // send response_cached back to client;
//                //     // send(client_fd, responsed_cached);
//                // }
//            }
//
//
//            // send(client_fd, responstd::cout << 'in cache, valid' << std::endl;sed_cached);
//        }
//    }
//    else {
//        std::cout << 'Not in cache, refetching' << std::endl;
//        // refetching
//        response_refetched = refetching(server_fd, request);
//        // insert
//        if (can_update(response_refetched) == true) {
//            cache.insert_cache(request_url, response_refetched);
//        }
//        // send(client_fd, responsed_cached);
//    }
//
//}

//Response refetching(int socket_fd, Request &req){
//    int len = strlen(req->get_request().c_str());
//    send(socket_fd, req->get_request().c_str(), len, 0);
//    std::cout << "Send successfull!" << std::endl;
//    std::vector<char> buffer(65536);
//    string check_end;
//    char *p = buffer.data();
//    int response_len = recv(socket_fd, p, 65536, 0);
//    if(response_len<0){
//        exit(1);
//    }
//    char *s = buffer.data();
//    std::cout << "Here receive:" << s << std::endl;
//    if (response_len > 0) {
//        Response *resp = new Response(s);
//    }
//    return Response;
//}

time_t get_expiration_time(Response &response){
    return 0;
}

void revalidation(){}

bool check_expired(){}

bool can_update(Response response) {
    std::map<std::string, std::string> header = response.get_header();
    std::string cache_control;
    auto it = header.find("Cache-Control");
    if (it != header.end()) {
        cache_control = it->second;
        if (cache_control.find("no-store") != string::npos ||
            cache_control.find("private") != string::npos ||
            cache_control.find("Authorization") != string::npos){
            return false;
        }
    }
    return true;
}

