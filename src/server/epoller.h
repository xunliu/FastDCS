/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// a simple wrapper of epoll
//
#ifndef FASTDCS_SERVER_EPOLLER_H_
#define FASTDCS_SERVER_EPOLLER_H_

#include <map>
#include <sys/epoll.h>
#include "src/base/common.h"

namespace fastdcs {
namespace server {

#define MAX_EVENTS 1024
#define EPOLLER_CALLBACK(x) void (*x) (poll_event_t *, poll_event_element_t *, struct epoll_event)
#define EPOLLER_TIMEOUT_CALLBACK(x) int (*x) (poll_event_t *)

#define ACCEPT_CB 0x01
#define CONNECT_CB 0x02

typedef struct poll_event_element poll_event_element_t;
typedef struct poll_event poll_event_t;

// element containing callbacks, user data and flags
struct poll_event_element {
  int fd; // the file descriptor
  EPOLLER_CALLBACK(write_callback);   // callback for write
  EPOLLER_CALLBACK(read_callback);    // callback for read
  EPOLLER_CALLBACK(close_callback);   // callback for close
  EPOLLER_CALLBACK(accept_callback);  // callback for accept
  EPOLLER_CALLBACK(connect_callback); // callback for connect
  void * data;        // user data for this element
  uint32_t events;    // epoll events flags
  uint32_t cur_event; // the event for which callback was initiated
  uint8_t cb_flags;   // only used to enable accept and listen callbacks
};
#define poll_event_element_s sizeof(poll_event_element_t)

struct poll_event {
  poll_event() {
    timeout_callback = NULL;
    data = NULL;
  }
  std::map<int /*socket_fd*/, poll_event_element_t *> table;
  int (*timeout_callback)(poll_event_t *); // timeout call back
  size_t timeout; // timeout duration
  int   epoll_fd; // epoll file descriptor
  void  *data;    // user data for poll_event
};
#define poll_event_s sizeof(poll_event_t)

class Epoller {
 public:
  explicit Epoller(int epoll_size = 256, int timeout = 1000);
  ~Epoller();

  // Function to allocate a new poll event element
  // @param fd the file descriptor to watch
  // @param events epoll events mask
  // @returns poll event element on success, returns NULL on failure
  poll_event_element_t* NewEventElement(int fd, uint32_t events);

  // Function to delete a poll event element
  // @param elem poll event element
  void DeleteEventElement(poll_event_element_t* elem);

  // Function to add a file descriptor to the event poll obeject
  // @note if add is performed on an fd already in poll_event, the flags are updated in the existing object
  // @param fd the file descriptor to be added
  // @param flags events flags from epoll
  // @param poll_element a poll event element pointer which is filled in by the function, set all function callbacks and cb_flags in this
  int AddToPollEvent(int fd, uint32_t flags, poll_event_element_t **poll_element);

  // Function to remove a poll event element from the given poll_event object
  // @param fd file descriptor which has to be removed
  int RemoveFromPollEvent(int fd);

  // Function which processes the events from epoll_wait and calls the appropriate callbacks
  // @param poll_event poll event object to be processed
  int PollEventProcess();
  void PollEventLoop();

  // 设置socket连接为非阻塞模式
  static void SetNonBlocking(int sockfd);

//  int (*timeout_callback_)(poll_event_t *); // callback for timeout

private:
  poll_event_t poll_event_; // 
  bool daemon_quit_;
};

} // namespace server
} // namespace fastdcs

#endif  // FASTDCS_SERVER_EPOLLER_H_
