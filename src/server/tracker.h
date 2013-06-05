/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#ifndef FASTDCS_SERVER_TRACKER_H_
#define FASTDCS_SERVER_TRACKER_H_

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
#include "src/server/connector.h"
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

//-----------------------------------------------------------------------------
// Implement FastDCS tracker server
//-----------------------------------------------------------------------------
class Tracker {
 public:
  static void Initialize(struct settings_s settings);
  static void Finalize();

  virtual void InitialTracker(struct settings_s settings);
  virtual void FinalizeTracker() = 0;

  // interface of Master/Worker close socket connect
  virtual bool SocketClose(int socket_fd);
  virtual bool AskTracker() = 0;

  virtual bool ProtocolProcess(int socket_fd, TrackerProtocol tracker_proto) = 0;
  virtual bool HeartBeat() = 0;

  // interface of Master/Worker close socket connect
  virtual bool SendProtocol(TrackerProtocol tracker_proto, int receiver_fd /*socket_fd*/);

  static void EpollAccentCallback(poll_event_t *, poll_event_element_t *, struct epoll_event);
  static void EpollReadCallback(poll_event_t *, poll_event_element_t *, struct epoll_event);
  static void EpollCloseCallback(poll_event_t *, poll_event_element_t *, struct epoll_event);
  static int EpollTimeoutCallback(poll_event_t *poll_event);

 protected:
  void AddReceiveSocketBuffer(int socket_fd, std::string data);
  void AddSocketResource(int socket_fd, string socket_addr);
  void RemoveSocketResource(int socket_fd);

  void GetSocketEvent(std::queue<int /*socket_fd*/> &queue_receive, 
                      std::queue<int /*socket_fd*/> &queue_send);

  long RandomNum() { 
    srand((unsigned)time(NULL)); 
    return rand(); 
  };

  void GetSocketAddress(int socket_fd, string &serverIP, string &guestIP);

  void CheckIP(std::string address) {
    std::vector<std::string> ip_and_port;
    SplitStringUsing(address, ":", &ip_and_port);
    CHECK_EQ(2, ip_and_port.size());
  }

  bool IAmMaster() { return is_master_; }
  bool IAmWorker() { return !IAmMaster(); }
  bool IAmPrimaryMaster();
  bool IKnowPrimaryMaster() { return (!primary_sockaddr_.empty()); }

  void UpdateMasterConhash();

  bool MasterAskTracker();
  bool WorkerAskTracker();

  bool Election();

  // The next Master of the current Master server addresses
  string NextMasterAddress(string address);

  // Find the next Master host, there is no next Master host return '-1', 
  // returns the next Master host socket_fd
  int FindNextMaster();

  bool ConnectMaster(std::string master_address);

  // Changes in the Master unit
  bool MasterGroupChanged();

  void GetTrackerStatus();

  void AskTaskStatus();
  void GetTaskStatus(TrackerProtocol &tracker_proto);

  bool is_master_; 

  // Primary Masterer socket address of the server
  string primary_sockaddr_;

  // Worker the socket address of the server
  string worker_sockaddr_;

  std::map<int /*socket_fd*/, string /*id*/> socket_ids_;
  std::map<std::string /*socket_addr*/, TCPSocket*> master_sockets_;
  typedef std::map<std::string /*socket_addr*/, TCPSocket*>::iterator SocketAddressIter;

  std::map<string /*socket_addr*/, TrackerStatus> tracker_status_;

  // All of the socket file description
  std::map<int /*socket_fd*/, SignalingQueue*> send_buffers_;
  std::map<int /*socket_fd*/, SignalingQueue*> receive_buffers_;
  typedef std::map<int, SignalingQueue*>::iterator SocketBuffersIter;

// socket_id_ stores the map from active socket to id
  std::map<int /*socket_fd*/, Connector* /*socket and buffer*/> send_connects_;
  typedef std::map<int, Connector*>::iterator SocketConnectsIter;

  TrackerProtocol NewTrackerProtocol(ProtocolType protocol_type);

  Epoller* epoll_;
  
  bool daemon_quit_;
  Enviroment fdcs_env_;
  struct conhash_s* master_conhash_;
  struct node_s* master_conhash_nodes_;
  
  void HeartBeatSignal() { cond_heartbeat_.Signal(); };
  void SocketThreadSignal() { cond_socket_.Signal(); };

 private:
  pthread_t thread_heart_beat_;
  static void* CallHeartBeatThread(void* pvoid);
  void HeartBeatThread();
  ConditionVariable cond_heartbeat_; 
  Mutex mutex_heartbeat_;

  // Socket thread
  std::queue<int /*socket_fd*/> socket_send_events_;
  std::queue<int /*socket_fd*/> socket_receive_events_;
  static void* CallSocketThread(void* pvoid);
  void SocketThread();
  pthread_t thread_socket_;
  ConditionVariable cond_socket_; 
  Mutex mutex_socket_;
};
CLASS_REGISTER_DEFINE_REGISTRY(fastdcs_tracker_registry,
                               fastdcs::server::Tracker)

#define REGISTER_FASTDCS_TRACKER(tracker_name)          \
  CLASS_REGISTER_OBJECT_CREATOR(                        \
      fastdcs_tracker_registry,                         \
      fastdcs::server::Tracker,                         \
      #tracker_name,                                    \
      tracker_name)

#define CREATE_FASTDCS_TRACKER(tracker_name_as_string)  \
  CLASS_REGISTER_CREATE_OBJECT(                         \
      fastdcs_tracker_registry,                         \
      tracker_name_as_string)

} // namespace server
} // namespace fastdcs

#endif // FASTDCS_SERVER_TRACKER_H_
