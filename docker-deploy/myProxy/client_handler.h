#ifndef CLIENT_HANDLER
#define CLIENT_HANDLER

#include "config.h"
#include "http_message.h"

#define BUF_SIZE 65535
const char * LOG_PATH = "./proxy.log";
//const char * LOG_PATH = "/var/log/erss/proxy.log";

class ClientHandler {
 private:
  int log_id;
  int srcFd;
  int destFd;
  string src_ip;
  string cacheKey;
  HttpRequest req;
  HttpResponse res;
  HttpResponse cacheRes;
  vector<vector<char> > buf;
  string bufStr;
  Cache * cache;

 public:
  ClientHandler(int fd, string ip, Cache * c) :
      srcFd(fd),
      destFd(-1),
      src_ip(ip),
      cache(c) {}
  ~ClientHandler() {
    close(srcFd);
    if (destFd != -1) {
      close(destFd);
    }
  }
  void run();
  size_t recvReq();
  size_t recvResponse();
  void connectServer();
  void sendServer();
  // Util functions
  size_t sendBuf(int fd);
  size_t advRecv(int fd);
  size_t recvManyTimes(int fd);
  size_t recvChunk(int fd, size_t recvSize);
  void workConnect();
  void send404();
  void send200();
  void send400();
  void writeLog(string content);
};

/**
 * Run the whole process
 * till finish sending response to client
 * or remote server close the connection.
 */
void ClientHandler::run() {
  // Receive Request.
  if (recvReq() <= 0) {
    send400();
    return;
  }
  // CONNECT
  if (req.getMethod() == "CONNECT") {
    connectServer();
    workConnect();
  }
  // POST
  else if (req.getMethod() == "POST") {
    sendServer();
  }
  // GET
  else {
    // if request is cacheable
    if (req.isReqCacheable()) {
      cacheKey = req.getCacheKey();
      // if we can find response in cache
      if (cache->isInCache(cacheKey)) {
        cacheRes = cache->get(cacheKey);
        int cacheResult = cacheRes.isRespCacheable();
        if (cacheResult == NO_VALIDATION_INFO) {
          writeLog("in cache, requires validation");
          sendServer();
          // TODO :: Send original req to Server.
          return;
        }
        else if (cacheResult == VALID) {
          writeLog("in cache, valid");
          buf.clear();
          buf = cacheRes.getEntireMsg();

          // ID: Responding "RESPONSE"
          string content = "Responding \"" + cacheRes.getStartLine() + "\"";
          writeLog(content);

          sendBuf(srcFd);
          return;
        }
        // MUST_REVALIDATE
        else {
          vector<char> newReq =
              req.getNewRequest(cacheRes.getEtag(), cacheRes.getLastModified());
          buf.clear();
          buf.push_back(newReq);
          sendServer();
          return;
        }  // end else MUST_REVALIDATE
      }    // end ifInCache
    }      // end isReqCacheable

    writeLog("not in cache");
    sendServer();
  }  // end GET
}
// Else send the res in cache.

void ClientHandler::sendServer() {
  // TODO :: Not In Cache
  // If "POST" or Note find in cache.
  connectServer();
  // TODO :: Need to be elegant...
  if (sendBuf(destFd) <= 0) {
    std::cerr << "Fail to send request to Server." << endl;
    send400();
    return;
  }

  // ID: Requesting "REQUEST" from SERVER
  string content = "Requesting \"" + req.getStartLine() + "\" from " + req.getHost();
  writeLog(content);

  if (recvResponse() <= 0) {
    std::cerr << "Fail to recv from Server." << endl;
    send400();
    return;
  }
  res = HttpResponse(string(buf[0].data()));
  res.setEntireMsg(buf);

  // ID: Received "RESPONSE" from SERVER
  content = "Received \"" + res.getStartLine() + "\" from " + req.getHost();
  writeLog(content);

  if (res.getStatusCode() == "304") {
    buf.clear();
    buf = cacheRes.getEntireMsg();
  }
  if (req.isReqStoreable() && res.isRespStoreable()) {
    cache->put(cacheKey, res);
  }
  if (sendBuf(srcFd) <= 0) {
    std::cerr << "Fail to send request to Client." << endl;
    send400();
    return;
  }

  // ID: Responding "RESPONSE"
  content = "Responding \"" + res.getStartLine() + "\"";
  writeLog(content);
}
/** 
 * recvReq(): 
 * initial request from client
 * -> req object, and complete req message
*/
size_t ClientHandler::recvReq() {
  size_t recvSize;
  if ((recvSize = advRecv(srcFd)) > 0) {
    string temp = string(buf[0].data());
    req = HttpRequest(temp);
    log_id = req.getLogId();

    // receive request and write log
    string content =
        "\"" + req.getStartLine() + "\" from " + src_ip + " @ " + req.getReqDate();
    writeLog(content);
  }
  return recvSize;
}

/** 
 * recvResponse(): 
 * Recv Response from Server.
 */
size_t ClientHandler::recvResponse() {
  // Clear all req in the buf.
  buf.clear();
  return advRecv(destFd);
}

/**
 * To consider Content Length, chuncked 
 */
size_t ClientHandler::advRecv(int fd) {
  // Recv one time, get the content length by HTTPMessage.
  size_t recvSize;
  char singleBuff[BUF_SIZE];
  memset(singleBuff, 0, BUF_SIZE);
  if ((recvSize = recv(fd, singleBuff, BUF_SIZE, 0)) <= 0) {
    return recvSize;
  }
  vector<char> temp(singleBuff, singleBuff + recvSize);
  buf.push_back(temp);
  HttpMessage atLeastHeader = HttpMessage(string(temp.data()));

  size_t contentLength = atLeastHeader.isContentLengthValid();
  // 1. Chunk has highest priority.
  if (atLeastHeader.isChunkValid()) {
    return recvChunk(fd, recvSize);
  }
  size_t headerLength = atLeastHeader.getHeaderLength();
  // 2. Use Content-Length to end.
  while (recvSize < (headerLength + contentLength)) {
    int singleRecvSize;
    if ((singleRecvSize = recvManyTimes(fd)) <= 0) {
      //break;
    }
    recvSize += singleRecvSize;
  }
  return recvSize;
}

/**
 * For chunk: continue receive util encounter crlf.
 * Ref: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
 */
size_t ClientHandler::recvChunk(int fd, size_t recvSize) {
  const char * crlf = "0\r\n\r\n";
  while (true) {
    int singleRecvSize;
    if ((singleRecvSize = recvManyTimes(fd)) <= 0) {
      return recvSize;
    }
    recvSize += singleRecvSize;
    vector<char> temp = buf[buf.size() - 1];
    if (search(temp.begin(), temp.end(), crlf, crlf + strlen(crlf)) - temp.end() != 0) {
      break;
    }
  }
  return recvSize;
}

/**
 * Util Recv, help store buf.
 * Use char[] as buffer, then resize as vector, finally push back.
 */
size_t ClientHandler::recvManyTimes(int fd) {
  char singleBuff[BUF_SIZE];
  memset(singleBuff, 0, BUF_SIZE);
  int singleRecvSize;
  if ((singleRecvSize = recv(fd, singleBuff, BUF_SIZE, 0)) <= 0) {
    return singleRecvSize;
  }
  vector<char> temp(singleBuff, singleBuff + singleRecvSize);
  buf.push_back(temp);
  return singleRecvSize;
}

/**
 * For GET/POST - Send request and receive response.
 */
size_t ClientHandler::sendBuf(int fd) {
  size_t sendSize = 0;
  for (size_t i = 0; i < buf.size(); ++i) {
    int singleSendSize;
    if ((singleSendSize = send(fd, &buf[i].data()[0], buf[i].size(), MSG_NOSIGNAL)) <=
        0) {
      return -1;
    }
    sendSize += singleSendSize;
  }
  return sendSize;
}

/** 
 * Method: CONNECT 
 * Use select to build a tunnel 
 */
void ClientHandler::workConnect() {
  // Send 200 back to client.
  send200();
  char recv_buff[8192];
  memset(recv_buff, 0, sizeof(recv_buff));
  fd_set rfds;
  int fds[] = {srcFd, destFd};
  while (true) {
    FD_ZERO(&rfds);
    for (size_t i = 0; i < 2; ++i) {
      FD_SET(fds[i], &rfds);
    }
    int nfd = srcFd > destFd ? srcFd + 1 : destFd + 1;
    if (select(nfd, &rfds, NULL, NULL, NULL) == -1) {
      cerr << "Select: \n";
      return;
    }
    int recv_size;
    size_t i = 0;
    for (i = 0; i < 2; ++i) {
      if (FD_ISSET(fds[i], &rfds)) {
        if ((recv_size = recv(fds[i], recv_buff, sizeof(recv_buff), 0)) == 0) {
          cerr << "CONNECTION close." << endl;
          return;
        }
        if (recv_size < 0) {
          cerr << "Fail to receive data through tunnel." << endl;
          return;
        }
        // D("[CONNECT] receive sucessfully");
        break;
      }
    }
    if (send(fds[(i + 1) % 2], recv_buff, recv_size, MSG_NOSIGNAL) <= 0) {
      cerr << "Fail to send data through tunnel." << endl;
      return;
    }
  }

  writeLog("Tunnel closed");
}

/**
 * Util method: connect to the remote server
 * based on the host and port in the req.
*/
void ClientHandler::connectServer() {
  int status;
  struct addrinfo host_info;
  struct addrinfo * host_info_list;

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(
      req.getHost().c_str(), req.getPort().c_str(), &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << req.getHost() << "," << req.getPort() << ")" << endl;
    return;
  }  //if

  destFd = socket(host_info_list->ai_family,
                  host_info_list->ai_socktype,
                  host_info_list->ai_protocol);
  if (destFd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << req.getHost() << "," << req.getPort() << ")" << endl;
    return;
  }  //if

  char buf[1024];
  read(destFd, buf, sizeof(buf) - 1);
  status = connect(destFd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << req.getHost() << "," << req.getPort() << ")" << endl;
    return;
  }  //if
  freeaddrinfo(host_info_list);
}

void ClientHandler::send200() {
  const char * status200 = "HTTP/1.1 200 OK\r\n\r\n";
  if (send(srcFd, status200, strlen(status200), 0) <= 0) {
    cerr << "Fail to run CONNECT method" << endl;
    return;
  }
  D("Succeed to send 200 back to client");
}

void ClientHandler::send400() {
  const char * status400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
  if (send(srcFd, status400, strlen(status400), 0) <= 0) {
    cerr << "Fail to run CONNECT method" << endl;
    return;
  }
  D("Succeed to send 400 back to client");
}

void ClientHandler::writeLog(string content) {
  ofstream ofs;
  ofs.open(LOG_PATH, ofstream::out | ofstream::app);

  ofs << log_id << ": " << content << endl;
  ofs.close();
}
#endif
