/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h> 
#include "epoller.h"
#include "src/base/logging.h"
#include "src/utils/stl_util.h"

namespace fastdcs {
namespace server {

Epoller::Epoller(int epoll_size, int timeout) {
  LOG(INFO) << "Created a new poll event";
  CHECK_GT(MAX_EVENTS, epoll_size);

  poll_event_.timeout = timeout;
  poll_event_.epoll_fd = epoll_create(epoll_size);
}

Epoller::~Epoller() {
  LOG(WARNING) << "deleting poll event";

  STLDeleteValuesAndClear(&poll_event_.table);
  close(poll_event_.epoll_fd);
}

poll_event_element_t* Epoller::NewEventElement(int fd, uint32_t events) {
  LOG(INFO) << "Creating a new poll event element";
  poll_event_element_t *elem = (poll_event_element_t *)calloc(1, poll_event_element_s);
  if (elem) {
    elem->fd = fd;
    elem->events = events;
  }
  return elem;
}

void Epoller::DeleteEventElement(poll_event_element_t* elem) {
  LOG(INFO) << "Deleting a poll event element";
  free(elem);
}

int Epoller::AddToPollEvent(int fd, uint32_t flags, poll_event_element_t **poll_element) {
  poll_event_element_t *elem = NULL;
  std::map<int /*socket_fd*/, poll_event_element_t *>::iterator iter;
  iter = poll_event_.table.find(fd);
  if (poll_event_.table[fd]) {
    elem = (poll_event_element_t *)poll_event_.table[fd];
//    LOG(INFO) << "fd (" << fd << ") already added updating flags.";
    elem->events |= flags;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = fd;
    ev.events = elem->events;
    *poll_element = elem;
    return epoll_ctl(poll_event_.epoll_fd, EPOLL_CTL_MOD, fd, &ev);
  } else {
    elem = NewEventElement(fd, flags);
      poll_event_.table[fd] = elem;
//    LOG(INFO) << "Added fd(" << fd << ")";
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = fd;
    ev.events = elem->events;
    *poll_element = elem;
    return epoll_ctl(poll_event_.epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  }
}

int Epoller::RemoveFromPollEvent(int fd) {
  LOG(INFO) << "Epoller::RemoveFromPollEvent(" << fd << ")";
  if (poll_event_.table[fd]) {
    delete poll_event_.table[fd];
    poll_event_.table[fd] = NULL;
    poll_event_.table.erase(fd);
  }
  close(fd);
  epoll_ctl(poll_event_.epoll_fd, EPOLL_CTL_DEL, fd, NULL);
  return 0;
}

// Function which processes the events from epoll_wait and calls the appropriate callbacks
// @note only process events once if you need to use an event loop use poll_event_loop
// @param poll_event_ poll event object to be processed
int Epoller::PollEventProcess() {
  struct epoll_event events[MAX_EVENTS];
  int fds = epoll_wait(poll_event_.epoll_fd, events, MAX_EVENTS, poll_event_.timeout);
  if (fds == 0) {
//    LOG(INFO) << "event loop timed out\n";
    if (poll_event_.timeout_callback) {
      if (poll_event_.timeout_callback(&poll_event_)) {
        return -1;
      }
    }
  }

  for (int i = 0 ; i < fds; ++i) {
    poll_event_element_t* value = (poll_event_element_t *)poll_event_.table[events[i].data.fd];
    if (value != NULL) {
//      LOG(INFO) << "started processing for event id(" << i 
//                << ") and sock(" << events[i].data.fd << ")";
      // when data avaliable for read or urgent flag is set
      if ((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI)) {
        if (events[i].events & EPOLLIN) {
//          LOG(INFO) << "found EPOLLIN for event id(" << i 
//                    << ") and sock(" << events[i].data.fd << ")";
          value->cur_event &= EPOLLIN;
        } else {
          LOG(INFO) << "found EPOLLPRI for event id(" << i 
                    << ") and sock(" << events[i].data.fd << ")";
          value->cur_event &= EPOLLPRI;
        }
        /// connect or accept callbacks also go through EPOLLIN
        /// accept callback if flag set
        if ((value->cb_flags & ACCEPT_CB) && (value->accept_callback))
          value->accept_callback(&poll_event_, value, events[i]);
        /// connect callback if flag set
        if ((value->cb_flags & CONNECT_CB) && (value->connect_callback))
          value->connect_callback(&poll_event_, value, events[i]);
        /// read callback in any case
        if (value->read_callback)
          value->read_callback(&poll_event_, value, events[i]);
      }

      // when write possible
      if (events[i].events & EPOLLOUT) {
        LOG(INFO) << "found EPOLLOUT for event id(" << i 
                  << ") and sock(" << events[i].data.fd << ")";
        value->cur_event &= EPOLLOUT;
        if (value->write_callback)
          value->write_callback(&poll_event_, value, events[i]);
      }

      // shutdown or error
      if ( (events[i].events & EPOLLRDHUP) 
        || (events[i].events & EPOLLERR) 
        || (events[i].events & EPOLLHUP)) {
          if (events[i].events & EPOLLRDHUP) {
            LOG(INFO) << "found EPOLLRDHUP for event id(" << i 
                      << ") and sock(" << events[i].data.fd << ")";
            value->cur_event &= EPOLLRDHUP;
          } else {
            LOG(INFO) << "found EPOLLERR for event id(" << i 
                      << ") and sock(" << events[i].data.fd << ")";
            value->cur_event &= EPOLLERR;
          }
          if (value->close_callback) {
            value->close_callback(&poll_event_, value, events[i]);
          }
          RemoveFromPollEvent(value->fd);
      }
    } else {
      LOG(INFO) << "WARNING: NOT FOUND event id(" << i 
                << ") and sock(" << events[i].data.fd << ")";
    }
  } // for
  return 0;
}

// Function to start the event loop which monitors all fds and callbacks accordingly
// @note event loop runs indefinitely and can only be stopped by timeout callback, 
// so to process events only once use poll_event_process
void Epoller::PollEventLoop() {
  LOG(INFO) << "Entering the main event loop for epoll lib.";
  while (!PollEventProcess());
}

// 设置socket连接为非阻塞模式
void Epoller::SetNonBlocking(int sockfd) {
  int opts;

  opts = fcntl(sockfd, F_GETFL);
  if(opts < 0) {
    perror("fcntl(F_GETFL)\n");
    exit(1);
  }
  opts = (opts | O_NONBLOCK);
  if(fcntl(sockfd, F_SETFL, opts) < 0) {
    perror("fcntl(F_SETFL)\n");
    exit(1);
  }
}

} // namespace server
} // namespace fastdcs
