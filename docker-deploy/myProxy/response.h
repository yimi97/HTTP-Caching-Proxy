//
// Created by 徐颖 on 2/17/20.
//

#ifndef PROXY_RESPONSE_H
#define PROXY_RESPONSE_H

#include <string>
#include <map>

using namespace std;

class Response {
private:
    std::string response;

    std::string status_line;
    std::string protocol_vision; // usually HTTP/1.1
    std::string status_code; // indicating success or failure of the request.
                            // Common status codes are 200, 404, or 302
    std::string status_text; // A brief, purely informational, textual description of the status code
                            // to help a human understand the HTTP message

    std::map<std::string, std::string> header;
    std::string response_body;

public:
    Response(){}
    Response(char* s): response(s), status_text(""), status_code(""),
    protocol_vision(""), status_line(""), response_body(""){
        status_line = parse_status_line();
        protocol_vision = parse_protocol_vision();
        status_code = parse_status_code();
        status_text = parse_status_text();
        header = parse_header();

    }
    Response(Response &rhs){}
    Response &operator=(Response &rhs){ return *this; }


    std::string parse_status_line(){ return response.substr(0, response.find("\r\n")); }
    std::string parse_protocol_vision(){ return status_line.substr(0, status_line.find(" ")); }
    std::string parse_status_code(){
        std::string tmp = status_line.substr(status_line.find(" ") + 1);
        tmp = tmp.substr(0, tmp.find(" "));
        return tmp;
    }
    std::string parse_status_text(){
        std::string tmp = status_line.substr(status_line.find(" ") + 1);
        tmp = tmp.substr(tmp.find(" ") + 1);
        return tmp;
    }
    std::map<std::string, std::string> parse_header() {
        string content = response.substr(response.find("\r\n") + 2, response.find("\r\n\r\n"));
        while(content.find("\r\n")!=string::npos) {
            if(content.substr(content.find("\r\n"))=="\r\n") { break; }
            std::string header_line = content.substr(0, content.find("\r\n"));
            std::string key = header_line.substr(0, header_line.find(":"));
            std::string value = header_line.substr(header_line.find(":") + 1);
            header[key] = value;
            content = content.substr(content.find("\r\n") + 2);
        }
        return header;
    }

    std::map<std::string, std::string> get_header(){ return header; }
    std::string get_status_line(){ return status_line; }
    std::string get_protocol_vision(){ return protocol_vision; }
    std::string get_status_code(){ return status_code; }
    std::string get_status_text(){ return status_text; }
    std::string get_response_body(){ return response_body; }
};
#endif //PROXY_RESPONSE_H
