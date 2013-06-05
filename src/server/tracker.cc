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
#include "tracker.h"

namespace fastdcs {
namespace server {

using std::string;
using std::vector;
using namespace fastdcs;

CLASS_REGISTER_IMPLEMENT_REGISTRY(fastdcs_tracker_registry,
                                  fastdcs::server::Tracker)

scoped_ptr<Tracker>& GetTracker() {
  static scoped_ptr<Tracker> tracker;
  return tracker;
}

Tracker* CreateTracker(const char* class_name) {
  Tracker* class_object = NULL;
  class_object = CREATE_FASTDCS_TRACKER(class_name);
  if (class_object == NULL) {
    LOG(ERROR) << "Cannot create :" << class_name;
  }
  return class_object;
}

//-----------------------------------------------------------------------------
// Implementation of Tracker
//-----------------------------------------------------------------------------
bool Tracker::IAmPrimaryMaster() { 
  return (IAmMaster() & !primary_sockaddr_.empty()
          & (0 == primary_sockaddr_.compare(fdcs_env_.TrackerServer()))); 
}

/*static*/ 
void Tracker::Initialize(struct settings_s settings) {
  GetTracker().reset(CreateTracker(settings.class_factory));
  CHECK(GetTracker().get());

  CHECK_EQ(pthread_create(&GetTracker()->thread_heart_beat_, NULL, CallHeartBeatThread, NULL), 0);
  CHECK_EQ(pthread_create(&GetTracker()->thread_socket_, NULL, CallSocketThread, NULL), 0);

  GetTracker()->InitialTracker(settings);
}

/*static*/ 
void Tracker::Finalize() {
  LOG(WARNING) << "Tracker::Finalize()";
  GetTracker()->daemon_quit_ = true;

  conhash_fini(GetTracker()->master_conhash_);
  if (GetTracker()->master_conhash_nodes_) 
    delete []GetTracker()->master_conhash_nodes_;
  
  GetTracker()->FinalizeTracker();
}

/*virtual*/
void Tracker::InitialTracker(struct settings_s settings) {
  // Initialize
  epoll_ = new Epoller(settings.max_connections);
  daemon_quit_ = false;
  primary_sockaddr_ = "";
  fdcs_env_.Settings(settings);
  fdcs_env_.InitFdcsTimeDiffer();

  vector<string> master_addr;
  SplitStringUsing(fdcs_env_.TrackerGroup(), ";", &master_addr);
  int master_count = master_addr.size();

  master_conhash_ = conhash_init(NULL);
  master_conhash_nodes_ = new node_s[master_count];

  if (!IAmMaster()) {
    for (int index = 0; index < master_count; index ++) {
      conhash_set_node(&master_conhash_nodes_[index], master_addr[index].data(), 256);
      conhash_add_node(master_conhash_, &master_conhash_nodes_[index]);
    }

    LOG(INFO) << "conhash virtual nodes number " 
              << conhash_get_vnodes_num(master_conhash_);
  }
}

/*virtual*/ 
void Tracker::FinalizeTracker() {
  LOG(WARNING) << "Tracker::FinalizeTracker()";

  mutex_socket_.Lock();
  for (SocketBuffersIter it = send_buffers_.begin(); it != send_buffers_.end(); ++it) {
    if(it->second) it->second->Signal(0);
  }
  for (SocketBuffersIter it = receive_buffers_.begin(); it != receive_buffers_.end(); ++it) {
    if(it->second) it->second->Signal(0);   
  }

  STLDeleteValuesAndClear(&send_buffers_);
  STLDeleteValuesAndClear(&receive_buffers_);
  STLDeleteValuesAndClear(&master_sockets_);
  mutex_socket_.Unlock();

  void *ret = NULL;
  HeartBeatSignal();
  pthread_join(thread_heart_beat_, &ret);

  SocketThreadSignal();
  pthread_join(thread_socket_, &ret);

  if (epoll_)
    delete epoll_;
}

void Tracker::UpdateMasterConhash() {
  LOG(INFO) << "Tracker::UpdateMasterConhash()";
  vector<string> master_addr;
  SplitStringUsing(fdcs_env_.TrackerGroup(), ";", &master_addr);
  int master_count = master_addr.size();

  for (int index = 0; index < master_count; ++index) {
    string master_address = master_addr[index];
    if (master_sockets_[master_address]) {
      conhash_add_node(master_conhash_, &master_conhash_nodes_[index]);
      LOG(INFO) << "conhash_add_node " << master_address;
    } else {
      conhash_del_node(master_conhash_, &master_conhash_nodes_[index]);
      LOG(INFO) << "conhash_del_node " << master_address;
    }
  }

  LOG(INFO) << "master conhash virtual nodes number " 
            << conhash_get_vnodes_num(master_conhash_);
}

TrackerProtocol Tracker::NewTrackerProtocol(ProtocolType protocol_type) {
  TrackerProtocol tracker_proto;
  tracker_proto.set_id(RandomNum());
  tracker_proto.set_time(fdcs_env_.FdcsTime());
  tracker_proto.set_tracker_type(IAmMaster() ? MASTER_TYPE : WORKER_TYPE);
  tracker_proto.set_request_sock_addr(IAmMaster() ? fdcs_env_.TrackerServer() : worker_sockaddr_);
  tracker_proto.set_protocol_type(protocol_type);
  return tracker_proto;
}

bool Tracker::ConnectMaster(string master_address) {
  string tracker_group = fdcs_env_.TrackerGroup();
  if (std::string::npos == tracker_group.find(master_address)) {
    LOG(ERROR) << master_address << " not tracker group!";
    return false;
  }

  if (master_sockets_[master_address])
    return true;

  vector<string> ip_and_port;
  SplitStringUsing(master_address, ":", &ip_and_port);
  CHECK_EQ(2, ip_and_port.size());

  TCPSocket *socket = new TCPSocket();
  if (false == socket->Connect(ip_and_port[0].c_str(), atoi(ip_and_port[1].c_str()))) {
    LOG(INFO) << "can't connet master service " << master_address;
    delete socket;
    return false;
  }
  LOG(INFO) << "connet master service " << master_address;
  socket->SetBlocking(true);

  mutex_socket_.Lock();
  if (master_sockets_[master_address]) {
    delete master_sockets_[master_address];
    master_sockets_[master_address] = NULL;
  }
  master_sockets_[master_address] = socket;
  AddSocketResource(socket->Socket(), master_address);
  mutex_socket_.Unlock();

  return true;
}

void Tracker::GetSocketAddress(int socket_fd, string &serverIP, string &guestIP) {
  struct sockaddr_in server, guest; 
  char serv_ip[32];
  char guest_ip[32];
  socklen_t serv_len = sizeof(server);  
  socklen_t guest_len = sizeof(guest);
  getsockname(socket_fd, (struct sockaddr *)&server, &serv_len);
  getpeername(socket_fd, (struct sockaddr *)&guest, &guest_len);

  char szPeerAddress[32];
  sprintf(szPeerAddress, "%s:%d", inet_ntoa(guest.sin_addr), ntohs(guest.sin_port));  
  //Copy a string.the second parameter strSource Null-terminated source string  
  //printf("guest.sin_addr = %s", szPeerAddress);

  inet_ntop(AF_INET, &server.sin_addr, serv_ip, sizeof(serv_ip));  
  inet_ntop(AF_INET, &guest.sin_addr, guest_ip, sizeof(guest_ip));
  sprintf(serv_ip, "%s:%d", serv_ip, ntohs(server.sin_port));
  sprintf(guest_ip, "%s:%d", guest_ip, ntohs(guest.sin_port));

  serverIP = serv_ip;
  guestIP = szPeerAddress;
  LOG(INFO) << "serverIP = " << serverIP << ", guestIP = " << guestIP;
}

/*static*/
int Tracker::EpollTimeoutCallback(poll_event_t *poll_event) {
  // just keep a count
  if (!poll_event->data) {
    poll_event->data = calloc(1,sizeof(int));
  } else {
    // increment and print the count
    int * value = (int*)poll_event->data;
    *value += 1;
  }
  return 0;
}

/*static*/
void Tracker::EpollCloseCallback(poll_event_t *poll_event, 
              poll_event_element_t *elem, struct epoll_event ev) {
  LOG(WARNING) << "Tracker::EpollCloseCallback(" << elem->fd << ")";
  GetTracker()->SocketClose(elem->fd);
}

/*static*/
void Tracker::EpollAccentCallback(poll_event_t *poll_event, 
              poll_event_element_t *elem, struct epoll_event ev) {
  LOG(INFO) << "Tracker::EpollAccentCallback(" << elem->fd << ")";
  // accept the connection 
  struct sockaddr_in clt_addr;
  socklen_t clt_len = sizeof(clt_addr);
  int sock_client = accept(elem->fd, (struct sockaddr*) &clt_addr, &clt_len);
  Epoller::SetNonBlocking(sock_client);
//  LOG(INFO) << "Accent new socket " << sock_client;

  // Add new socket buffer resources
//  string serverIP, guestIP;
//  GetTracker()->GetSocketAddress(sock_client, &serverIP, &guestIP);
  GetTracker()->AddSocketResource(sock_client, "");
}

/*static*/
void Tracker::EpollReadCallback(poll_event_t *poll_event, 
              poll_event_element_t *elem, struct epoll_event ev) {
//  LOG(INFO) << "Tracker::EpollReadCallback(" << elem->fd << ")";
  string message;
  char buffer[4096] = {0};

  int len = read(elem->fd, buffer, sizeof(buffer));
  if (len == -1 && errno != EAGAIN) {
    LOG(WARNING) << "socket read error!";
    GetTracker()->SocketClose(elem->fd);
    return;
  } else if (len > 0) {
    message.append(buffer, len);
    while(len > 0) {
      len = read(elem->fd, buffer, sizeof(buffer));
      if (len > 0)
        message.append(buffer, len);
    }
//    LOG(INFO) << "message length = " << message.length();
    GetTracker()->AddReceiveSocketBuffer(elem->fd, message);
  }
}

/*virtual*/ 
bool Tracker::SendProtocol(TrackerProtocol tracker_proto, int receiver_fd) {
  LOG(INFO) << "Tracker::SendProtocol() receiver_fd = " << receiver_fd;
  string response_socket_addr = tracker_proto.response_sock_addr();
  if (IAmMaster()) {
    if (response_socket_addr.empty())
      response_socket_addr = fdcs_env_.TrackerServer();
    else {
      response_socket_addr += ";";
      response_socket_addr += fdcs_env_.TrackerServer();
    }
  }

  tracker_proto.set_response_sock_addr(response_socket_addr);  
  std::string proto_buff;
  tracker_proto.SerializeToString(&proto_buff);

  SocketBuffersIter iter = send_buffers_.find(receiver_fd);
  if (iter != send_buffers_.end()) {
    iter->second->Add(proto_buff);
    LOG(INFO) << "socket = " << iter->first << ", socket_buff_size = " << proto_buff.length() 
              << ", request_socket_addr = " << tracker_proto.request_sock_addr() 
              << ", response_socket_addr = " << tracker_proto.response_sock_addr();
    mutex_socket_.Lock();
    socket_send_events_.push(iter->first);
    mutex_socket_.Unlock();  
  } else {
    LOG(ERROR) << "receiver_fd = " << receiver_fd << ", " 
               << socket_ids_[receiver_fd] << " not found!";
    return false;
  }
  SocketThreadSignal();
  return true;
}

/*virtual*/ 
bool Tracker::SocketClose(int socket_fd) {
  LOG(WARNING) << "Tracker::SocketClose(" << socket_fd << ")";

  string service_addr, guest_addr;
  GetSocketAddress(socket_fd, service_addr, guest_addr);
  MutexLocker locker(&mutex_socket_);

  if (master_sockets_[guest_addr]) {
    delete master_sockets_[guest_addr];
    master_sockets_[guest_addr] = NULL;

    if (0 == primary_sockaddr_.compare(guest_addr)) {
      LOG(WARNING) << "Primary masterer server [" << primary_sockaddr_ << "] offline!";
      primary_sockaddr_ = "";
    } else {
      LOG(WARNING) << "Master server["<< guest_addr << "] offline!";
      UpdateMasterConhash();
      MasterGroupChanged();
    }
  }

  // Release the SOCKET connection and cache resources
  RemoveSocketResource(socket_fd);

  return true;
}

void Tracker::AddReceiveSocketBuffer(int socket_fd, string data) {
  LOG(INFO) << "Tracker::AddReceiveSocketBuffer(" << socket_fd << ")";
  MutexLocker locker(&mutex_socket_);
  SocketBuffersIter iter = receive_buffers_.find(socket_fd);
  if (iter != receive_buffers_.end()) {
    int size = iter->second->Add(data);
    socket_receive_events_.push(socket_fd);
    LOG(INFO) << "receive_buffers_[" << socket_fd << "] add size = " << size;
    SocketThreadSignal();
  } else {
    LOG(ERROR) << "receive_buffers_ not find!";
  }
}

void Tracker::AddSocketResource(int socket_fd, string socket_addr) {
  LOG(INFO) << "Tracker::AddSocketResource(" << socket_fd << ", " << socket_addr << ")";
  MutexLocker locker(&mutex_socket_);

  string service_addr, guest_addr;
  GetSocketAddress(socket_fd, service_addr, guest_addr);

  socket_ids_[socket_fd] = socket_addr;

  if (send_buffers_.find(socket_fd) == send_buffers_.end()) {
//    LOG(INFO) << "new socket send buffer.";
    send_buffers_[socket_fd] = new SignalingQueue(fdcs_env_.SocketBuffSize());
    Connector* send_connect = new Connector();
    send_connect->Initialize(send_buffers_[socket_fd], socket_fd);
    send_connects_[socket_fd] = send_connect;
  }

  if (receive_buffers_.find(socket_fd) == receive_buffers_.end()) {
//    LOG(INFO) << "new socket receive buffer.";
    receive_buffers_[socket_fd] = new SignalingQueue(fdcs_env_.SocketBuffSize());
  }

   // epoll add this socket event
  uint32_t flags = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
  poll_event_element_t *event = NULL;
  // add file descriptor to poll event
  epoll_->AddToPollEvent(socket_fd, flags, &event);
  // set function callbacks 
  event->read_callback = EpollReadCallback;
  event->close_callback = EpollCloseCallback;
}

void Tracker::RemoveSocketResource(int socket_fd) {
  LOG(INFO) << "Tracker::RemoveSocketResource(" << socket_fd << ")";
  MutexLocker locker(&mutex_socket_);

  LOG(ERROR) << "socket_ids_[socket_fd] = " << socket_ids_[socket_fd];
  socket_ids_.erase(socket_fd);

  SocketBuffersIter iter = send_buffers_.find(socket_fd);
  if (iter != send_buffers_.end()) {
    if (iter->second)
      delete(iter->second);
    send_buffers_.erase(socket_fd);
  }

  iter = receive_buffers_.find(socket_fd);
  if (iter != receive_buffers_.end()) {
    if (iter->second)
      delete(iter->second);
    receive_buffers_.erase(socket_fd);
  }

  SocketConnectsIter connect_iter = send_connects_.find(socket_fd);
  if (connect_iter != send_connects_.end()) {
    if (connect_iter->second)
      delete(connect_iter->second);
    send_connects_.erase(socket_fd);
  }

/*  SocketsIter socket_iter = worker_sockets_.find(socket_fd);
  if (socket_iter != worker_sockets_.end()) {
    if (socket_iter->second)
      delete(socket_iter->second);*/
//    worker_sockets_.erase(socket_fd);
//  }
}

void Tracker::GetSocketEvent(std::queue<int /*socket_fd*/> &queue_receive, 
                             std::queue<int /*socket_fd*/> &queue_send) {
  MutexLocker locker(&mutex_socket_);
  while (!socket_receive_events_.empty()) {
    int socket_fd = socket_receive_events_.front();
    socket_receive_events_.pop();
    queue_receive.push(socket_fd);
  }

  while (!socket_send_events_.empty()) {
    int socket_fd = socket_send_events_.front();
    socket_send_events_.pop();
    queue_send.push(socket_fd);
  }
}

/*static*/
void* Tracker::CallSocketThread(void* pvoid) {
  sleep(3);
  GetTracker()->SocketThread();
  return NULL;
}

void Tracker::SocketThread() {
  while(!daemon_quit_) {
    MutexLocker locker(&mutex_socket_);
    while ((socket_send_events_.size() == 0)
        && (socket_receive_events_.size() == 0)) {
      if (daemon_quit_) return;
//      LOG(INFO) << "cond_socket_.Wait(&mutex_socket_, 5000);";
      cond_socket_.Wait(&mutex_socket_, 5000);
    }

    std::queue<int> queue_receive, queue_send;
    GetSocketEvent(queue_receive, queue_send);

    while (!queue_receive.empty()) {
      int socket_fd = queue_receive.front();
      queue_receive.pop();

      if (receive_buffers_[socket_fd]) {
        string message;
        receive_buffers_[socket_fd]->Remove(&message);
//        LOG(INFO) << "socket = " << socket_fd << " receive size = " << size;

        TrackerProtocol tracker_proto;
        tracker_proto.ParseFromString(message);
        // Delay settings determine whether SOCKET receives data over the network, 
        // over is discarded.
        int32 timeout = fdcs_env_.FdcsTime() - tracker_proto.time();
        LOG(INFO) << "FdcsTime() = " << fdcs_env_.FdcsTime()
                   << ", tracker_proto.time() = " << tracker_proto.time();
        if (timeout >= fdcs_env_.NetworkTimeout()) {
          LOG(WARNING) << "Protocol receive buffers timeout " << timeout << "(s)";
//        continue;
        }
        ProtocolProcess(socket_fd, tracker_proto);
      } else {
        LOG(ERROR) << "not find receive_buffers_ by (" << socket_fd << ")";
      }
    }

    while (!queue_send.empty()) {
      int socket_fd = queue_send.front();
      queue_send.pop();

      if (send_connects_[socket_fd]) {
        int size = send_connects_[socket_fd]->Send();
        LOG(INFO) << "socket = " << socket_fd << " Send return size = " << size;
      } else {
        LOG(INFO) << "not find send_connects_ " << socket_fd;
      }
    }
  }

  LOG(INFO) << "SocketThread quit!";
}

/*static*/
void* Tracker::CallHeartBeatThread(void* pvoid) {
  sleep(3);
  GetTracker()->HeartBeatThread();
  return NULL;
}

void Tracker::HeartBeatThread() {
  int heart_beat_interval = fdcs_env_.HeartBeatInterval();

  while(!daemon_quit_) {
    if (daemon_quit_) return;
    HeartBeat();
    MutexLocker locker(&mutex_heartbeat_);
    cond_heartbeat_.Wait(&mutex_heartbeat_, heart_beat_interval * 1000);
  }
  LOG(INFO) << "quit HeartBeatThread!";
}

// The next Master of the current Master server addresses
string Tracker::NextMasterAddress(string address) {
//  LOG(INFO) << "Master::NextMasterAddress(" << address << ")";
  string next_addr;
  string tracker_group = fdcs_env_.TrackerGroup();
  vector<string> master_addresses;
  SplitStringUsing(tracker_group, ";", &master_addresses);
  CHECK_GT(master_addresses.size(), 0);

  int size = master_addresses.size();
  for (int i = 0; i < size; i ++) {
    next_addr = master_addresses.at(i);
    if (0 == next_addr.compare(address)) {
      if (i+1 == master_addresses.size())
        next_addr = master_addresses[0];
      else 
        next_addr = master_addresses[i+1];
      break;
    }
  }
//  LOG(INFO) << "address = " << address << ", next_addr = " << next_addr;

  return next_addr;
}

// Find the next Master host, there is no next Master host return '-1', 
// returns the next Master host socket_fd
int Tracker::FindNextMaster() {
  LOG(INFO) << "Master::FindNextMaster()";
  string next_master_addr = NextMasterAddress(fdcs_env_.TrackerServer());

  while (true) {
    if (0 == next_master_addr.compare(fdcs_env_.TrackerServer())) {
      // There is no next Master Server, then your own is the primary Master
      LOG(INFO) << "Only one master server : " << next_master_addr;
      return -1;
    }

    if (true == ConnectMaster(next_master_addr)) {
      return master_sockets_[next_master_addr]->Socket();
    } else {
      next_master_addr = NextMasterAddress(next_master_addr);
    }
  }
  return -1;
}

// Changes in the Master unit
bool Tracker::MasterGroupChanged() {
  if ((IAmMaster() && !IAmPrimaryMaster()) || (!IAmMaster()))
    return false;
  
  LOG(INFO) << "Master::MasterGroupChanged()";
  int next_master_fd = FindNextMaster();
  if (next_master_fd == -1) {
    LOG(INFO) << "Only one master server!";
    return true;
  }
  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(MASTER_GROUP_CHANGED);
  string online_master;

  vector<string> master_addr;
  SplitStringUsing(fdcs_env_.TrackerGroup(), ";", &master_addr);
  int master_count = master_addr.size();

  for (int index = 0; index < master_count; ++index) {
    string master_address = master_addr[index];
    if (master_sockets_[master_address]) {
      if (online_master.empty()) {
        online_master += master_address;
      } else {
        online_master += ";" + master_address;
      }
    }
  }
  tracker_proto.set_request_args(online_master);

  return SendProtocol(tracker_proto, next_master_fd);
}

void Tracker::AskTaskStatus() {
  LOG(INFO) << "Tracker::AskTaskStatus()";
  if (IKnowPrimaryMaster()) {
    TrackerProtocol tracker_proto;
    tracker_proto = NewTrackerProtocol(ASK_TASK_STATUS);
    SendProtocol(tracker_proto, master_sockets_[primary_sockaddr_]->Socket());
  }
}

void Tracker::GetTaskStatus(TrackerProtocol &tracker_proto) {
  LOG(INFO) << "Tracker::GetTaskStatus()";

  KeyValuesPair *key_values_pair = tracker_proto.add_key_values_pairs();
  key_values_pair->set_key("TrackerStatus");
  for (SocketAddressIter iter = master_sockets_.begin(); 
       iter != master_sockets_.end(); ++iter) {
    TrackerStatus tracker;
    if (iter->second) {
      LOG(INFO) << "GetTaskStatus " << iter->first;
      tracker.set_socket_addr(iter->first);

      int size = tracker.ByteSize();
      void *buffer = malloc(size);
      tracker.SerializeToArray(buffer, size);

      LOG(INFO) << "encoded_msg = " << buffer;
      key_values_pair->add_value(buffer, size);
    }
  }
}

void Tracker::GetTrackerStatus() {
  long num_procs;
  long page_size;
  long num_pages;
  long free_pages;
  long long  mem;
  long long  free_mem;
  num_procs = sysconf (_SC_NPROCESSORS_CONF);
  printf ("CPU 个数为: %ld 个", num_procs);
  page_size = sysconf (_SC_PAGESIZE);
  num_pages = sysconf (_SC_PHYS_PAGES);
  free_pages = sysconf (_SC_AVPHYS_PAGES);
  mem = (long long) ((long long)num_pages * (long long)page_size);
  mem /= (1024*1024);
  free_mem = (long long)free_pages * (long long)page_size;
  free_mem /= (1024*1024);
  printf ("A total of %lld MB of physical memory, free physical memory:%lld MB\n", mem, free_mem);
  
  fastdcs::TrackerStatus tracker_status;
  tracker_status.set_socket_addr(IAmPrimaryMaster() ? fdcs_env_.TrackerServer() : worker_sockaddr_);
  tracker_status.set_active(true);
  tracker_status.set_process_num(num_procs);
  tracker_status.set_total_memory(mem);
  tracker_status.set_free_memory(free_mem);

  tracker_status_[fdcs_env_.TrackerServer()] = tracker_status;
}

}  // namespace server
}  // namespace fastdcs
