#include <vector>
#include <string>
#include <stdio.h>
#include <string.h>

size_t my_recv(int fd, vector<vector<char>> &mybuffer){
    std::vector<char> first_buf(65535);
    char *p = first_buf.data();
    size_t len = 0;
    memset(first_buf, 0, 65535);
    if((len = recv(fd, p, 65535, 0))<=0){
        return len;
    }
    mybuffer.push_back(first_buf);
    // mybuffer is the buffer taking the whole response
    string first_str(first_buf.begin(), first_buf.end());
    header_str = first_str.substr(0, first_str.find("\r\n\r\n") + 4);
    auto pos_chk = header_str.find("chunked");
    auto pos_clen = header_str.find("Content-Length: ");
    if(pos_chk != nullptr) {
        // chunked
        while(true) {
            size_t once_len;
            if((once_len = continue_recv(fd)) <= 0) {
                return len;
            }
            len += once_len;
            vector<char> last_buf = mybuffer[mybuffer.size()-1];
            if (strstr(last_buf.data(), "0\r\n\r\n") != string::nops) {
                break;
            }
        }
        return len;
    }
    else if(pos_clen != NULL) {
        string content = header_str.substr(header_str.find("Content-Length: "));
        content = content.substr(16, content.find("\r\n"));
        size_t length = (size_t)atoi(content.c_str());
        while (len < (length + header_str.size())){
            size_t once_len;
            if((once_len = continue_recv(fd)) <= 0){
                //break;
            }
            len += once_len;
        }
        return len;
    }    
    return 0;
}

size_t continue_recv(int fd, vector<vector<char>> &mybuffer) {
    vector<char> continue_buf(65535);
    char *p = continue_buf.data();
    memset(continue_buf, 0, 65535);
    size_t once_len;
    if((once_len = recv(fd, p, 65535, 0))<=0){
        return once_len;
    }
    mybuffer.push_back(continue_buf);
    free(&continue_buf);
    return once_len;
}

size_t my_send(int fd) {
    size_t len = 0;
    int num = 0;
    while(num != mybuffer.size()) {
        //int once_len = send(fd, mybuffer[i].data(), mybuffer[i].size(), 0));
        int once_len = send(fd, &mybuffer[i].data()[0], mybuffer[i].size(), MSG_NOSIGNAL));
        if (once_len <=0){
            return once_len;
        }
        len += once_len;
        num += 1;
    }
    return len;
}



/*
 *mybuffer.clear();
 *convert response to vector<vector>
 */
// mybuffer.clear();

void resp_to_buf(vector<vector<char>> &mybuffer, Response response){
    string resp = response.get_response();
    size_t temp_size = 0;
    size_t full_size = 65535;
    size_t remain_size = response.size();
    string content;
    while(remain_size > 0) {
        if(remain_size < full_size) {
            content = response.substr(temp_size, remain_size);
            vector<char> temp(content.begin() , content.end());
            mybuffer.push_back(temp);
            break;
        }
        content = response.substr(temp_size, full_size);
        vector<char> temp(content.begin() , content.end());
        mybuffer.push_back(temp);
        temp_size = temp_size + full_size;
        remain_size = remain_size - full_size;    
    }
}

void send_cached_response(){
    string resp_str = response_cached.get_response();
    mybuffer.clear();
    resp_to_buf(mybuffer, resp_str);
    my_send(fd, mybuffer);
}


/* add send_400 to the behind of all recv(server_fd) if code == 400*/
void send_400(){
    string header_502 = "HTTP/1.1 400 Bad Request\r\n\r\n";
    int status = send(client_fd, header_502.c_str(), header.size(), 0);
    if(status<=0){   
    }
}
string s(first_buf);
string code = s.substr(s.find(" ") + 1, 3);
if (code == "400") {
    send_400();
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Received 304 \"NOT modified\" from Server" << endl;
    log_flow.close();
    return true;
}

/*add send_502 to the behind of all recv(server_fd) if status<0 send 502*/
void send_502(){
    string header_502 = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    int status = send(client_fd, header_502.c_str(), header.size(), 0);
    if(status<=0){
    }
}
if(len<0){
    send_502();
}


/*
  If have bad request, return 400 to client
*/
void return400(int client_fd) {
  std::cout << "Bad request" << std::endl;
  std::string header("HTTP/1.1 400 Bad Request\r\nContent-Length: "
                     "38\r\nConnection: close\r\nContent-Type: "
                     "text/html\r\n\r\n<html><body>Bad Request</body></html>\n");
  int len = send(client_fd, header.c_str(), header.length(), MSG_NOSIGNAL);
}


/*
  If have bad response, return 502 to client
*/
void return502(int client_fd) {
  std::cout << "Bad request" << std::endl;
  std::string header("HTTP/1.1 502 Bad Gateway\r\nContent-Length: "
                     "38\r\nConnection: close\r\nContent-Type: "
                     "text/html\r\n\r\n<html><body>Bad Gateway</body></html>\n");
  int len = send(client_fd, header.c_str(), header.length(), MSG_NOSIGNAL);
}
