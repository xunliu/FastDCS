/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h> // for inet_aton, inet_ntop
#include "src/utils/stl_util.h"
#include "src/utils/split_string.h"
#include "src/server/connector.h"

namespace fastdcs {
namespace server {

using std::string;
using std::vector;
using namespace fastdcs;

//-----------------------------------------------------------------------------
// Implementation of Connector
//-----------------------------------------------------------------------------
Connector::Connector() {
  Clear();
}

void Connector::Initialize(SignalingQueue *queue, int socket_fd) {
  CHECK_NOTNULL(queue);
  CHECK_GT(socket_fd, 0);
  queue_ = queue;
  socket_fd_ = socket_fd;
}

void Connector::Clear() {
  message_.clear();
  message_size_ = 0;
  bytes_count_ = 0;
}

int Connector::SendFinal() {
  uint32 this_send;
  message_size_ = 0;
  bytes_count_ = 0;

  // send a FINAL message with message_size_ of 0
  while (bytes_count_ < sizeof(message_size_)) {
    this_send = send(socket_fd_, 
                     reinterpret_cast<char*>(&message_size_) + bytes_count_, 
                     sizeof(message_size_) - bytes_count_, 0);
    if (this_send < 0) {
      LOG(ERROR) << "Socket send error.";
      return -1;
    }
    bytes_count_ += this_send;
  }
  return 0;
}

int Connector::Send() {
  // If message_ is empty, read a new message from queue_
  // until the queue_ is empty and will have no more messages
  if (message_.empty()) {
    message_size_ = queue_->Remove(&message_, false);
    if (message_.empty()) {
      if (queue_->EmptyAndNoMoreAdd()) {
        return SendFinal();
      }
      return 1;
    } else {
//      message_.insert(0, reinterpret_cast<char*>(&message_size_),
//                      sizeof(message_size_));
    }
  }

  // For each message, send message size first, then send the
  // content of this message.
  // Because sock_->Send() may actually send only part of data in the given
  // buffer, this_send is used to record how many byte have been sent each time,
  // and accumulated to bytes_count_.
  // So, when (bytes_count_ < sizeof(message_size_)), we continue to send
  // message_size_, else, send message_.data()
  uint32 this_send = 0;
  this_send = send(socket_fd_, message_.data() + bytes_count_, 
                   message_.size() - bytes_count_, 0);
  if (this_send < 0) {
    LOG(ERROR) << "Socket send error.";
    return -1;
  }
  bytes_count_ += this_send;

  // this message complete
  if (bytes_count_ == message_.size()) {
    Clear();
  }

  return 1;
}

int Connector::Receive() {
  // For each message, receive its size first, then receive its content.
  // Similar with Send(), this_receive is used to record how many bytes have
  // been received this time, and accumulated to bytes_count_.
  // printf("bytes_count_ = %d, message_size_ = %d\n", bytes_count_, message_size_);

  uint this_receive = 0;
  if (bytes_count_ >= sizeof(message_size_) && message_size_ == 0) {
    return 0;
  } else {
    this_receive = recv(socket_fd_, message_buf_, 4096, 0);
  }

  if (this_receive < 0) {
    LOG(ERROR) << "Socket receive error.";
    return -1;
  }

  if (this_receive > 0) {
    uint pointer = 0;
    uint len = 0;
    while (pointer < this_receive) {
      if (bytes_count_ < sizeof(message_size_)) {
        len = std::min(sizeof(message_size_) - bytes_count_,
                       this_receive - pointer);
        memcpy(reinterpret_cast<char*>(&message_size_) + bytes_count_,
               message_buf_ + pointer, len);
      } else {
        len = std::min(sizeof(message_size_) + message_size_ - bytes_count_,
                       this_receive - pointer);
        message_.append(message_buf_ + pointer, len);
      }
      bytes_count_ += len;
      pointer += len;
      if (bytes_count_ >= sizeof(message_size_) &&
          message_.size() ==  message_size_) {
        if (message_size_ == 0) return 0;
        if (queue_->Add(message_) < 0) return -1;
        Clear();
      }
    }
  }

  return 1;
}

} // namespace server
} // namespace fastdcs
