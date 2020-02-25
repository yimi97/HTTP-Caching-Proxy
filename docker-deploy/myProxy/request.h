//
// Created by 徐颖 on 2/15/20.
//
#include <string>
#include <map>
#ifndef PROXY_REQUEST_H
#define PROXY_REQUEST_H

using namespace std;
class Request{

private:
    std::string request;

    std::string request_line;
    std::string method;
    std::string request_uri;
    std::string http_version;
    std::string full_url;

    std::string host;
    std::string port;

    std::map<std::string, std::string> header;
    std::string request_body;
    int uid;

public:
    Request(){}
    Request(std::string s, int id): request(s), method(""), request_line(""), request_uri(""),
    http_version(""), host(""), port(""), request_body(""), uid(id){
        this->request_line = parse_request_line();
        this->method = parse_method();
        this->request_uri = parse_request_uri();
        this->http_version = parse_http_version();
        if (request.find("Host:") != std::string::npos) {
            this->host = parse_host();
            std::cout << "Constructing...Host" << std::endl;
            this->port = parse_port();
            if (request_uri.find(host) == std::string::npos) {
                this->full_url = host + request_uri;
            } else {
                this->full_url = request_uri;
            }
        } else {
            this->full_url = request_uri;
        }
    }
    Request(const Request &rhs){}
    Request &operator=(const Request &rhs){ return *this; }

    std::string get_request(){ return this->request; }
    std::string get_request_line(){ return request_line; }
    std::string get_method(){ return method; }
    std::string get_full_url(){ return full_url; }
    std::string get_request_uri(){ return request_uri; }
    std::string get_http_versions(){ return http_version; }
    std::string get_port(){ return port; }
    std::string get_host(){ return host; }
    std::string get_request_body(){ return request_body; }
    std::map<std::string, std::string> get_header(){ return header; }

    std::string parse_request_line(){
        return request.substr(0, request.find("\r\n"));
    }
    std::string parse_method(){
        return request_line.substr(0, request_line.find(" "));
    }
    std::string parse_request_uri(){
        std::string my_uri = request_line.substr(request_line.find(" ") + 1, request_line.find("\r\n"));
        my_uri = my_uri.substr(0, my_uri.find(" "));
        if(my_uri.find(":") != string::npos){
            my_uri = my_uri.substr(0,my_uri.find(":"));
        }

        return my_uri;
    }
    std::string parse_http_version(){
        std::string my_version = request_line.substr(request_line.find(" ") + 1, request_line.find("\r\n"));
        my_version = my_version.substr(my_version.find(" ") + 1);
        return my_version;
    }
    std::string parse_host(){
        std::string my_host = request.substr(request.find("Host: ") + 6);
        my_host = my_host.substr(0, my_host.find("\r\n"));
        std::cout << "Constructing..." << my_host << std::endl;
        if(my_host.find(":") != string::npos){
            my_host = my_host.substr(0,my_host.find(":"));
        }
        return my_host;
    }
    std::string parse_port(){
//        std::string my_port = request.substr(request.find("\r\n") + 4);
        std::string my_port = request.substr(request.find("Host: ") + 6);
        my_port = my_port.substr(0, my_port.find("\r\n"));
        if(my_port.find(":") != string::npos){
            my_port = my_port.substr(my_port.find(":") + 1);
            return my_port;
        }
        return "";
    }
    std::map<std::string, std::string> parse_header() {
        string content = request.substr(request.find("\r\n") + 2, request.find("\r\n\r\n"));
        while(content.find("\r\n")!=string::npos) {
            if(content.substr(content.find("\r\n"))=="\r\n") {break;}
            std::string header_line = content.substr(0, content.find("\r\n"));
            std::string key = header_line.substr(0, header_line.find(":"));
            std::string value = header_line.substr(header_line.find(":") + 1);
            header[key] = value;
            content = content.substr(content.find("\r\n") + 2);
        }
        return header;
    }


};
#endif //PROXY_REQUEST_H
