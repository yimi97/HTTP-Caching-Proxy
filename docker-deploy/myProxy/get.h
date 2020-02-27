#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <iostream>
#include <locale>
#include <iomanip>
#include <sstream>
#include <time.h>
#include <typeinfo>
#include <string.h>
using namespace std;

LRUCache cache(99999);

void GET(Request request, int remote_fd, int client_fd){
    std::map<std::string, std::string> request_header = request.get_header();
    std::string request_cache_control;
    std::string request_url = request.get_full_url();
    
    Response response_cached;
    std::map<std::string, std::string> response_cached_header;
    std::string response_cached_cache_control;
    
    Response response_refetched;
    std::map<std::string, std::string> response_refetched_header;

    bool response_in_cache = cache.search_cache(request_url);
    bool expired;
    bool revalidation(Response response, Request request);
    bool valid;
    int status;
    time_t expired_time;
    time_t now = time(0);
    bool can_update(Response response);
    bool check_expired(std::map<std::string, std::string> header);
    
    // in cache
    if(response_in_cache == true){
        response_cached = cache.get_cache(request_url);
        response_cached_header = response_cached.get_header();

        expired_time = get_expiration_time(response_cached_header);
        // expired
        if (difftime(now, expired_time)>0) {
            std::cout << 'In cache, but expired at ' << asctime(gmtime(&expired_time)) << std::endl;
            // refetching
                response_refetched = refetching(remote_fd, request);
            // update cache
                if (can_update(response_refetched) == true) {
                    cache.update_cache(request_url, response_refetched);
                }
                send(client_fd, response_refetched);
                return;
        }
        else {// if not expired
            // request cache-control
            auto req_it = request_header.find("Cache-Control");
            if (req_it != request_header.end()) {
                request_cache_control = req_it->second;
                // no-cahce -> revalidate
                if (request_cache_control.find("no-cache") != string::npos) {
                    // revalidate
                    std::cout << 'in cache, but need to revalidate' << std::endl;
                    valid = revalidation(server_fd, response_cached, request);
                    if(valid == true) {
                        std::cout << 'in cache, revalidation succeed, valid' << std::endl;
                        // send(client_fd, response_cached);
                        return;
                    }
                    else {
                        std::cout << 'in cache, revalidation failed, refetching' << std::endl;
                        // refetching
                        response_refetched = refetching(remote_fd, request);
                        // update
                        if (can_update(response_refetched) == true) {
                            cache.update_cache(request_url, response_refetched);
                        }
                        // send(client_fd, response_refetched);
                        return;
                    }

                }
                // no-store -> refetching
                if (request_header.find("no-store")!=request_header.end()){
                    std::cout << 'in cache, but request saying no-store, refetching' << std::endl;
                    // refetching
                    response_refetched = refetching(remote_fd, request);
                    // update
                    if (can_update(response_refetched) == true) {
                        cache.update_cache(request_url, response_refetched);
                    }
                    // send(client_fd, response_refetched);
                    return;
                }
            }
            // response cache-control
            auto resp_it = response_cached_header.find("Cache-Control");
            if (resp_it != response_cached_header.end()) {
                response_cached_cache_control = resp_it->second;
                if (response_cached_cache_control.find("no-cache") != string::npos ||
                    response_cached_cache_control.find("must-revalidate") != string::npos ||
                    response_cached_cache_control.find("proxy-revalidate") != string::npos ) {
                    std::cout << 'in cache, but need to revalidate' << std::endl;
                    valid = revalidation(server_fd, response_cached, request);
                    if (valid == true) {
                        std::cout << 'in cache, revalidation succeed, valid' << std::endl;
                        // send(client_fd, response_cached);
                        return;
                    }
                    else {
                        std::cout << 'in cache, revalidation failed, refetching' << std::endl;
                        // refetching
                        response_refetched = refetching(remote_fd, request);
                        // update
                        if (can_update(response_refetched) == true) {
                            cache.update_cache(request_url, response_refetched);
                        }
                        // send(client_fd, response_refetched);
                        return;
                    }
                }
            }
            std::cout << 'in cache, valid' << std::endl;
            // send(client_fd, response_cached);
        }
    }
    else {
        std::cout << 'Not in cache, refetching' << std::endl;
        // refetching
        response_refetched = refetching(remote_fd, request);
        // insert
        if (can_update(response_refetched) == true) {
            cache.insert_cache(request_url, response_refetched);
        }
        // send(client_fd, response_cached);
    }
}

Response refetching(int socket_fd, Request &req){
    int len = strlen(req->get_request().c_str());
    send(socket_fd, req->get_request().c_str(), len, 0);
    std::cout << "Send successfull!" << std::endl;
    std::vector<char> buffer(65536);
    string check_end;
    char *p = buffer.data();
    int response_len = recv(socket_fd, p, 65536, 0);
    if(response_len<0){
        exit(1);
    }
    char *s = buffer.data();
    std::cout << "Here receive:" << s << std::endl;
    if (response_len > 0) {
        Response *resp = new Response(s);
    }
    return Response;
}

time_t get_time(string date_or_exp) {
    int n = date_or_exp.length();
    char char_array[n + 1];
    strcpy(char_array, date_or_exp.c_str());
    const char *time_details = char_array;
    struct tm tm;
    strptime(time_details, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    time_t t = mktime(&tm);
    return t;
}

time_t get_expiration_time(std::map<std::string, std::string> header){
    auto cc = header.find("Cache-Control");
    auto exp = header.find("Expires");
    time_t now = time(0);
    // check expire
    if(exp != header.end()){
        time_t expire_time = get_time((exp->second).substr(1));
        return expire_time;
    }
    // check max-age
    else if(cc != header.end()){
        auto date = header.find("Date");
        auto age = header.find("Age");
        std::string cache_control = cc->second;
        std::string max_age = cache_control.substr(cache_control.find("max-age=") + 8, cache_control.find("max-age=") + 16);
        if (max_age.find(",")!=string::npos){
            max_age = max_age.substr(0, max_age.find(","));
        }
        time_t ma_time = (time_t)atoi(max_age.c_str());
        if (date != header.end()){
            time_t date_time = get_time((date->second).substr(1));
            return date_time + ma_time;
        }else if (age != header.end()) {
            string age_str = age->second;
            time_t age_time = (time_t)atoi(age_str.c_str());
            return now - (age_time - ma_time);
        } 
    }
    return 0; 
}


bool can_update(Response response) {
    std::map<std::string, std::string> header = response.get_header();
    auto cc = header.find("Cache-Control");
    auto exp = header.find("Expires");
    auto date = header.find("Date");
    auto age = header.find("Age");

    std::string cache_control;

    if (cc != header.end()){
        cache_control = cc->second;
        if (cache_control.find("no-store") != string::npos ||
            cache_control.find("private") != string::npos ||
            cache_control.find("Authorization") != string::npos){
            return false;
        }
        if(cache_control.find("max-age")!=string::npos) {
            if (date == header.end() && age == header.end() && exp == header.end()) {
                return false;
            }
        } else {
            if (exp == header.end()){
                return false;
            }
        }
    }else{
        if(exp == header.end()){
            return false;
        }
    }
    return true;
}


bool revalidation(int remote_fd, Response response, Request request){
    // construct revalidation request
    int status;
    std::map<std::string, std::string> resp_header = response.get_header();
    std::map<std::string, std::string> req_header = request.get_header();
    std::string req;
    std::string req_content;
    std::string newheader;
    auto it_etag = resp_header.find("Etag");
    if (it_etag != resp_header.end()) {
        newheader += string("\r\n") + string("If-None-Match:") + it_etag->second;
    }
    else {
        auto it_mod = resp_header.find("Last-Modified");
        if (it_mod != resp_header.end()) {
            newheader += string("\r\n")+ string("If-Modified-Since:") + it_mod->second;
        }
        else {
            return false;
        }
    }
    req = request.get_request();
    req_content = req.substr(0, req.find("\r\n\r\n")+4);
    req_content.insert(req_content.find("\r\n\r\n"), newheader);

    //send revalidation
    status = send(remote_fd, req_content, strlen(req_content), 0);
    if (status < 0) {return;}
    std::vector<char> buffer(999);
    char* p = buffer.data();
    status = recv(remote_fd, p, 999, 0);
    if (status < 0) {return;}
    std::string s(p);

    // identify code
    string code = s.substr(s.find(" ") + 1, 3);
    if (code == "200") {
        return true;
    }
    return false;
}


//vector<char> recvHeader(int fd){
//    vector<char> res;
//    int flag = 0;
//    char buffer[1];
//
//    while(true){
//        memset(buffer, 0, 1);
//        if (recv(fd, buffer, 1, 0) == 0){
//            res.resize(0, 0);
//            return res;
//        }
//        res.push_back(buffer[0]);
//
//        if (buffer[0] == '\r'){
//            if(flag == 0 || flag == 2){
//                flag++;
//            }
//            else {
//                flag = 1;
//            }
//        }
//        else if(buffer[0] == '\n'){
//            if (flag == 1 || flag == 3){
//                flag++;
//            }
//            else {
//                flag = 0;
//            }
//        }
//        else {
//            flag = 0;
//        }
//        if (flag == 4){
//            break;
//        }
//    }
//    res.push_back('\0');
//    return res;
//}
//
//vector<char> parseHeader(int fd, vector<char> & client_request_header) {
//    vector<char> body;
//    char * pos1 = strstr(client_request_header.data(), "Content-Length:");
//    char * pos2 = strstr(client_request_header.data(), "Transfer-Encoding:");
//    if (pos2 != NULL){
//        client_request_header.pop_back();
//        return recvHeader(fd);
//    }
//    else if (pos1 != NULL){
//        char * length = strstr(pos1, "\r\n");
//        char arr[10];
//        strncpy(arr, pos1+16, length-pos1-16);
//        arr[length-pos1-16] = 0;
//        int num = atoi(arr);
//        vector<char> buf(num+1, 0);
//        if(num != 0){
//            recv(fd, buf.data(), num, MSG_WAITALL);
//            client_request_header.pop_back();
//        }
//        return buf;
//    }
//    return body;
//}
//
//void sendAll(int fd, vector<char> & target, int size){
//    int sum = 0;
//    while(sum != size){
//        int i = send(fd, target.data() + sum, size-sum, 0);
//        if (i == -1) {
//            break;
//        }
//        sum += i;
//    }
//}

char* Proxy::header_recv(int sender_fd) {
    std::vector<char> buffer(buffer_size);
    char *p = buffer.data();
    int len = 0;
    len = recv(sender_fd, p, buffer_size, 0);
    if (len < 0) {
        exit(1);
    }
    string cur_string(buffer.begin(), buffer.end());
    int total = len;
    while (cur_string.find("\r\n\r\n") == string::npos) {
        // if not finish
        buffer.resize(total + buffer_size);
        p = buffer.data() + total;
        len = recv(sender_fd, p, buffer_size, 0);
        if (len < 0){
            exit(1);
        }
        total += len;
        cur_string = buffer.data();
    }
    return buffer.data();
    // memory leak, free buffer!
}

vector<char> Proxy::header_recv(int sender_fd){
    std::vector<char> buffer(2000);
    char *p = buffer.data();
    int len = 0;
    len = recv(sender_fd, p, buffer_size, 0);
    if (len < 0) {
        exit(1);
    }
    string s(buffer.begin(), buffer.end());
    if(s.find("\r\n\r\n")!=string::npos){
        s = s.substr(0, s.find("\r\n\r\n")+4);
        return vector<char> header(s.begin(),s.end());
    }
    //return no header
}
vector<char> Proxy::continue_recv(int sender_fd, vector<char> &header){
    vector<char> body;
    auto pos_chk = strstr(header.date(), "chunked");
    auto pos_clen = strstr(header.date(), "Content-Length:");
    if(pos_chk != NULL) {
        header.pop_back();
        return handle_chunk(sender_fd);
    }
    else if(pos_clen != NULL) {
        string content(header.begin(), header.end());
        content = content.substr(content.find("Content-Length: "));
        content = content.substr(16, content.find("\r\n"));
        int length = (int)atoi(content.c_str());
        vector<char> newbuffer(length+1, 0);
        if (length != 0){
            recv(fd, newbuffer.data(), length, MSG_WAITALL);
            header.pop_back();
        }
        return newbuffer;
    }
    return body;
}

vector<char> Proxy::handle_chunk(int sender_fd) {

}