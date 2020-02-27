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
    string remain = resp;
    size_t temp_size = 0;
    size_t full_size = 65535;
    size_t remain_size = resp_size;
    vector<char> temp;
    size_t resp_size = resp.size();
    while(remain_size > 0) {
        if(remain_size < full_size) {
            temp = resp(resp.begin()+temp_size, resp.end());
        }
        temp = resp(remain.begin() + temp_size, remain.begin() + temp_size + full_size);  
        mybuffer.push_back(temp);
        temp_size = temp_size + full_size;
        remain_size = remain_size - full_size;    
    }
}


