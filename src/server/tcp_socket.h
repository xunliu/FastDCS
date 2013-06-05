/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// TCPSocket is a simple wrapper around a socket. It supports
// only TCP connections.
//
#ifndef FASTDCS_SERVER_TCP_SOCKET_H_
#define FASTDCS_SERVER_TCP_SOCKET_H_

#include <sys/socket.h>
#include <string>

#include "src/base/common.h"

namespace fastdcs {
namespace server {

// TCPSocket is a simple wrapper around a socket. It supports
// only TCP connections.
class TCPSocket {
 public:
  // ctor and dctor
  TCPSocket();
  ~TCPSocket();

  // Return value of following functions:
  // true for success and false for failure
  // connect to a given server address
  bool Connect(const char * ip, uint16 port);

  // bind on the given IP ans PORT
  bool Bind(const char * ip, uint16 port);

  // listen
  bool Listen(int max_connection);

  // wait for a new connection
  // new SOCKET, IP and PORT will be stored to socket, ip_client and port_client
  bool Accept(TCPSocket * socket,
              std::string * ip_client,
              uint16 * port_client);

  // SetBlocking() is needed refering to this example of epoll:
  // http://www.kernel.org/doc/man-pages/online/pages/man4/epoll.4.html
  bool SetBlocking(bool flag);

  // Shut down one or both halves of the connection.
  // If ways is SHUT_RD, further receives are disallowed.
  // If ways is SHUT_WR, further sends are disallowed.
  // If ways is SHUT_RDWR, further sends and receives are disallowed.
  bool ShutDown(int ways);

  // close socket
  void Close();

  // send/receive data:
  // return number of bytes read or written if OK, -1 on error
  // caller is responsible for checking that all data has been sent/received,
  // if not, extra send/receive should be invoked
  int Send(const char * data, int len_data);
  int Receive(char * buffer, int size_buffer);

  // return socket's file descriptor
  int Socket() const;

  std::string Address() { return sockaddr_; };
 private:
  int socket_;
  std::string sockaddr_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocket);
};

} // namespace server
} // namespace fastdcs

#endif  // FASTDCS_SERVER_TCP_SOCKET_H_
