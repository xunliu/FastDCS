/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include "src/server/tcp_socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

namespace fastdcs {
namespace server {

typedef struct sockaddr_in SAI;
typedef struct sockaddr SA;

TCPSocket::TCPSocket() {
  // init socket
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    LOG(FATAL) << "Can't create new socket.";
  }
}

// dctor
TCPSocket::~TCPSocket() {
  Close();
}

bool TCPSocket::Connect(const char * ip, uint16 port) {
//  LOG(INFO) << "connect to " << ip << ":" << port;
  SAI sa_server;
  sa_server.sin_family = AF_INET;
  sa_server.sin_port   = htons(port);

  if (0 < inet_pton(AF_INET, ip, &sa_server.sin_addr) &&
      0 <= connect(socket_, reinterpret_cast<SA*>(&sa_server),
                   sizeof(sa_server))) {
    char addr[128];
    sprintf(addr, "%s:%d", ip, port);
    sockaddr_ = addr;
    LOG(INFO) << "socket_fd(" << socket_ << ") Connect to " << sockaddr_;
    return true;
  }

  LOG(ERROR) << "socket_fd(" << socket_ << ") Failed connect to " 
             << ip << ":" << port;
  sockaddr_ = "";
  return false;
}

bool TCPSocket::Bind(const char * ip, uint16 port) {
  SAI sa_server;
  sa_server.sin_family = AF_INET;
  sa_server.sin_port   = htons(port);

  // Set the socket option to avoid address errors
  int on = 1;
  if((setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {  
      LOG(ERROR) << "setsockopt failed";  
      return false;
  }

  if (0 < inet_pton(AF_INET, ip, &sa_server.sin_addr) &&
      0 <= bind(socket_, reinterpret_cast<SA*>(&sa_server),
                sizeof(sa_server))) {
    char addr[128];
    sprintf(addr, "%s:%d", ip, port);
    sockaddr_ = addr;
    
    return true;
  }

  LOG(ERROR) << "socket_fd(" << socket_ << ")Failed bind on " 
             << ip << ":" << port;
  sockaddr_ = "";
  return false;
}

bool TCPSocket::Listen(int max_connection) {
  if (0 <= listen(socket_, max_connection)) {
    return true;
  }

  LOG(ERROR) << "socket_fd(" << socket_ << ")Failed listen on socket fd: " 
             << socket_;
  sockaddr_ = "";
  return false;
}

bool TCPSocket::Accept(TCPSocket * socket, std::string * ip, uint16 * port) {
  int sock_client;
  SAI sa_client;
  socklen_t len = sizeof(sa_client);

  sock_client = accept(socket_, reinterpret_cast<SA*>(&sa_client), &len);
  if (sock_client < 0) {
    LOG(ERROR) << "socket_fd(" << socket_ << ")Failed accept connection on " 
               << ip << ":" << port;
    return false;
  }

  char tmp[INET_ADDRSTRLEN];
  const char * ip_client = inet_ntop(AF_INET,
                                     &sa_client.sin_addr,
                                     tmp,
                                     sizeof(tmp));
  CHECK_NOTNULL(ip_client);
  ip->assign(ip_client);
  *port = ntohs(sa_client.sin_port);
  socket->socket_ = sock_client;

  return true;
}

bool TCPSocket::SetBlocking(bool flag) {
  int opts;

  if ((opts = fcntl(socket_, F_GETFL)) < 0) {
    LOG(ERROR) << "socket_fd(" << socket_ << ")Failed to get socket status.";
    return false;
  }

  if (flag) {
    opts |= O_NONBLOCK;
  } else {
    opts &= ~O_NONBLOCK;
  }

  if (fcntl(socket_, F_SETFL, opts) < 0) {
    LOG(ERROR) << "socket_fd(" << socket_ << ")Failed to set socket status.";
    return false;
  }

  return true;
}

bool TCPSocket::ShutDown(int ways) {
  return 0 == shutdown(socket_, ways);
}

void TCPSocket::Close() {
  if (socket_ >= 0) {
    LOG(INFO) << "socket_ = " << socket_ << ", sockaddr_ = " << sockaddr_;
    CHECK_EQ(0, close(socket_));
    socket_ = -1;
    sockaddr_ = "";
  }
}

int TCPSocket::Send(const char * data, int len_data) {
  if (socket_ < 0) {
    LOG(ERROR) << "socket_ = " << socket_;
  }
  return send(socket_, data, len_data, MSG_NOSIGNAL);
}

int TCPSocket::Receive(char * buffer, int size_buffer) {
  return recv(socket_, buffer, size_buffer, 0);
}

int TCPSocket::Socket() const {
  return socket_;
}

} // namespace server
} // namespace fastdcs
