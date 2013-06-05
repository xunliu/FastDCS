/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// Connector is used by SocketCommunicator.
// A Connector is used for connecting a TCPSocket to a SignalingQueue.
//  1. Send() reads messages from SignalingQueue, serializes these messages
//     to byte stream, and send it through TCPSocket
//  2. Receive() receives byte stream from TCPSocket, convert it back to
//     messages, and write them to SignalingQueue
//
#ifndef FASTDCS_SERVER_CONNECTOR_H_
#define FASTDCS_SERVER_CONNECTOR_H_

#include <map>
#include <string>
#include <vector>
#include <sys/time.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>

#include "src/base/scoped_ptr.h"
#include "src/base/class_register.h"
#include "src/base/common_define.h"
#include "src/libconhash/conhash.h"
#include "src/system/mutex.h"
#include "src/server/epoller.h"
#include "src/server/signaling_queue.h"
#include "src/server/tcp_socket.h"
#include "src/server/enviroment.h"
#include "src/server/tracker_protocol.pb.h"
#include "src/utils/split_string.h"

namespace fastdcs {
namespace server {

using std::string;
using namespace fastdcs;

class Connector {
 public:
  Connector();
  void Initialize(SignalingQueue *queue, int socket_fd);

  // return:
  //  1: success
  //  0: queue_ is empty and no more
  //  -1: error
  int Send();

  // return:
  //  1: success
  //  0: no more messages
  //  -1: error
  int Receive();

 private:
  void Clear();
  int SendFinal();        // send a FINAL message

  SignalingQueue *queue_;
  int socket_fd_;
  std::string message_;
  char message_buf_[4096];
  uint32 message_size_;   // size of this message
  uint32 bytes_count_;    // bytes have been sent/received
};

}  // namespace server
}  // namespace fastdcs

#endif  // FASTDCS_SERVER_CONNECTOR_H_
