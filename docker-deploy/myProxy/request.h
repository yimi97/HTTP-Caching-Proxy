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
    std::string host_ip;
    string recv_time;
    size_t content_length;
    size_t header_length;
    std::map<std::string, std::string> header;
    int uid;

public:
    Request(){}
    ~Request() {
    }
    Request(std::string s, int id):
    request(s), method(""),
    request_line(""),
    request_uri(""),
    http_version(""),
    host(""), port(""),
    uid(id), content_length(0),
    header_length(0),
    host_ip(""), recv_time(""){
        time_t now = time(0);
        recv_time = asctime(gmtime(&now));

        this->request_line = parse_request_line();
        this->method = parse_method();
        this->request_uri = parse_request_uri();
        this->http_version = parse_http_version();
        if (request.find("Host:") != std::string::npos) {
            this->host = parse_host();
            this->port = parse_port();
            if (request_uri.find(host) == std::string::npos) {
                this->full_url = host + request_uri;
            } else {
                this->full_url = request_uri;
            }
        } else {
            this->full_url = request_uri;
        }
        cout << "uri: " << request_uri << endl;
        cout << "full url: " << full_url << endl;
        size_t pos = request.find("Content-Length: ");
        if (pos != string::npos) {
            string content = request.substr(request.find("Content-Length: ") + 16);
            content = content.substr(0, content.find("\r\n"));
            content_length = (size_t)atoi(content.c_str());
        } else {
            content_length = 0;
        }
        header_length = request.length();

    }

    Request(const Request &rhs){}
    Request &operator=(const Request &rhs){ return *this; }

    string get_recv_time(){ return this->recv_time; }
    std::string get_request(){ return this->request; }
    std::string get_request_line(){ return request_line; }
    std::string get_method(){ return method; }
    std::string get_full_url(){ return full_url; }
    size_t get_content_len(){ return content_length; }
    size_t get_header_len(){ return header_length; }
    std::string get_port(){ return port; }
    std::string get_host(){ return host; }
    std::map<std::string, std::string>* get_header(){ return &header; }


    std::string parse_request_line(){
        return request.substr(0, request.find("\r\n"));
    }
    std::string parse_method(){
        return request_line.substr(0, request_line.find(" "));
    }
    std::string parse_request_uri(){
        std::string my_uri = request_line.substr(request_line.find(" ") + 1);
        my_uri = my_uri.substr(0, my_uri.find(" "));
        return my_uri;
    }
    std::string parse_http_version(){
        std::string my_version = request_line.substr(request_line.find(" ") + 1);
        my_version = my_version.substr(my_version.find(" ") + 1);
        return my_version;
    }
    std::string parse_host(){
        std::string my_host = request.substr(request.find("Host: ") + 6);
        my_host = my_host.substr(0, my_host.find("\r\n"));
        if(my_host.find(":") != string::npos){
            my_host = my_host.substr(0,my_host.find(":"));
        }
        return my_host;
    }
    std::string parse_port(){
        std::string my_port = request.substr(request.find("Host: ") + 6);
        my_port = my_port.substr(0, my_port.find("\r\n"));
        if(my_port.find(":") != string::npos){
            my_port = my_port.substr(my_port.find(":") + 1);
            return my_port;
        }
        return "80";
    }

    /*
     * Mi Yi modified.
     */
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
