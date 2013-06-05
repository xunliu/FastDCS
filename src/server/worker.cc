/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <vector>

#include "src/base/logging.h"
#include "src/utils/string_codec.h"
#include "src/utils/split_string.h"
#include "src/server/protofile.h"
#include "src/server/worker.h"

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

REGISTER_FASTDCS_TRACKER(Worker);

/*virtual*/ 
void Worker::InitialTracker(struct settings_s settings) {
  LOG(INFO) << "Worker::InitialTracker()";
  Tracker::InitialTracker(settings);

  int master_fd = FindMaster();
  CHECK_GT(master_fd, 0);

  string serverIP, guestIP;
  GetSocketAddress(master_fd, worker_sockaddr_, guestIP);
  LOG(INFO) << "worker socket address : " << worker_sockaddr_;

  cond_computing_ = new ConditionVariable[settings.computing_threads];
  mutex_computing_ = new Mutex[settings.computing_threads];
  computing_tid_ = new pthread_t[settings.computing_threads];
  thread_index_ = new thread_index_s[settings.computing_threads];
  for (int i = 0; i < settings.computing_threads; ++i) {
    thread_index_[i].index_ = i;
    thread_index_[i].worker_ = this;
    CHECK_EQ(pthread_create(&computing_tid_[i], NULL, CallComputing, (void*)&thread_index_[i]), 0);
  }

  poll_event_element_t *p = NULL;
  // add listenfd to poll event
  uint32_t flags = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
  epoll_->AddToPollEvent(master_fd, flags, &p);
  // set callbacks
  p->read_callback = EpollReadCallback;
  p->close_callback = EpollCloseCallback;

  // start the event loop
  epoll_->PollEventLoop();
}

/*virtual*/ 
void Worker::FinalizeTracker() {
  LOG(INFO) << "Worker::FinalizeTracker()";
  for (std::map<int, string>::iterator it = socket_ids_.begin(); it != socket_ids_.end(); ++it) {
    LOG(INFO) << "socket_ids_.socket_ = " << it->second;
  }

  ComputingSignal();
  void *ret = NULL;
  for (int i = 0; i < fdcs_env_.ComputingThreads(); ++i)
    pthread_join(computing_tid_[i], &ret);

  if (cond_computing_)
    delete []cond_computing_;
  if (mutex_computing_)
    delete []mutex_computing_;
  if (computing_tid_)
    delete []computing_tid_;
  if (thread_index_)
    delete []thread_index_;

  Tracker::FinalizeTracker();
}

/*virtual*/ 
bool Worker::ProtocolProcess(int socket_fd, TrackerProtocol tracker_proto) {
  LOG(INFO) << "Worker::ProtocolProcess(" << tracker_proto.protocol_type() << ")";

  string request_socket_addr = tracker_proto.request_sock_addr();
  string response_socket_addr = tracker_proto.response_sock_addr();
  string response_result = tracker_proto.response_result();
  LOG(INFO) << "request_socket_addr = " << request_socket_addr 
            << ",response_socket_addr = " << response_socket_addr
            << ",response_result = " << response_result;
  // Begin communication protocol processing
  switch(tracker_proto.protocol_type()) {       
    case WORKER_ASK_TRACKER: {
      LOG(INFO) << "[WORKER_ASK_TRACKER] protocol ...";
      primary_sockaddr_ = response_result;
      LOG(INFO) << "Tracker server : " << primary_sockaddr_;

      // Synchronous master server time
      std::map<std::string, std::string> key_values;
      for (int i = 0; i < tracker_proto.key_values_pairs_size(); ++i) {
        KeyValuesPair key_values_pairs = tracker_proto.key_values_pairs(i);
        if (0 == key_values_pairs.key().compare(KEYVALUEPAIRS_FDCS_TIME)) {
          ReadRecord(&key_values_pairs, &key_values);
          break;      
        }
      }
      int32 fdcs_time = KeyToInt32(key_values[KEYVALUEPAIRS_FDCS_TIME]);
      LOG(INFO) << "fdcs_time : " << fdcs_time;
      fdcs_env_.CalcFdcsTimeDiffer(fdcs_time);

      ConnectMaster(primary_sockaddr_);
      HeartBeatSignal();
    }
    break;
    case WORKER_REQUEST_TASK:
      LOG(INFO) << "[WORKER_REQUEST_TASK] protocol ...";
      SaveNewTask(tracker_proto);
      break;
    default:
      LOG(ERROR) << "undefine protocol type : " << tracker_proto.protocol_type();
      break;
  }
  return true;
}

/*virtual*/ 
bool Worker::HeartBeat() {
//    LOG(INFO) << "Master::HeartBeat()";
  if (IKnowPrimaryMaster()) {
    RequestTask();
  } else {
    AskTracker();     
  }

  // To prevent unusual circumstances, blocking network
  //sleep(1);
  
  return true;
}

/*virtual*/
bool Worker::AskTracker() {
  LOG(INFO) << "Worker::AskTracker()";

  int master_fd = FindMaster();
  if (-1 == master_fd) {
    LOG(INFO) << "Can't find master server.";
    return false;
  }

  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(WORKER_ASK_TRACKER);
  return SendProtocol(tracker_proto, master_fd);
}

bool Worker::RequestTask() {
  if (fdcs_env_.TasksSize() > 0) {
    return false;
  }
  LOG(INFO) << "Worker::RequestTask()";
  int master_fd = FindOwnerMaster();
  if (-1 == master_fd) {
    LOG(ERROR) << "can't connect owner master and tracker server.";
    return false;
  }

  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(WORKER_REQUEST_TASK);
  tracker_proto.set_request_args(Int32ToKey(fdcs_env_.TaskDuplicate()));
  // report complete task
  KeyValuesPair *key_values_pair = tracker_proto.add_key_values_pairs();
  key_values_pair->set_key(KEYVALUEPAIRS_TASK_COMPLETE);
  for (int i = 0; i < fdcs_env_.CompletedTasksSize(); ++i) {
    FdcsTask *task = fdcs_env_.CompletedTasks(i);
    LOG(INFO) << "Completed task id = " << task->task_id();
    if (task) WriteRecord(key_values_pair, *task);  
  }
  fdcs_env_.ClearCompletedTasks();

  return SendProtocol(tracker_proto, master_fd);
}

bool Worker::SaveNewTask(TrackerProtocol tracker_proto) {
  if (fdcs_env_.TasksSize() > 0) {
    return false;
  }
  LOG(INFO) << "Worker::SaveNewTask()";

  std::vector<FdcsTask> new_tasks;
  for (int i = 0; i < tracker_proto.key_values_pairs_size(); ++i) {
    KeyValuesPair key_values_pair = tracker_proto.key_values_pairs(i);
    if (0 == key_values_pair.key().compare(KEYVALUEPAIRS_TASK)) {
      ReadRecord(&key_values_pair, &new_tasks);
    }
  }

  for (int index = 0; index < new_tasks.size(); ++index) {
    LOG(INFO) << "new task id : " << new_tasks[index].task_id() << ", " << new_tasks[index].lease_time();
//    fdcs_env_.AddTask(new_tasks[index]);
  }

  if (new_tasks.size() > 0) {
    fdcs_env_.UpdateTasks(new_tasks);
    LOG(INFO) << "IdleTasksSize : " << fdcs_env_.IdleTasksSize();
    LOG(INFO) << "TasksSize : " << fdcs_env_.TasksSize();
    ComputingSignal();
  }

  return true;
}

/*static*/
void* Worker::CallComputing(void* pvoid) {
  thread_index_t *thread_index = (thread_index_t *)pvoid;
  thread_index->worker_->Computing((void*)thread_index->index_);
  return NULL;
}

void Worker::Computing(void* pvoid) {
  int thread_index = (int)(pvoid);
//  LOG(INFO) << "Computing thread " << thread_index << " start.";
  while (!daemon_quit_) {
    MutexLocker locker(&mutex_computing_[thread_index]);
    while (fdcs_env_.TasksSize() == 0) {
      if (daemon_quit_) return;
      LOG(INFO) << "cond_computing_.Wait('mutex_computing_()');";
      HeartBeatSignal();
      cond_computing_[thread_index].Wait(&mutex_computing_[thread_index], 5000);
    }

    FdcsTask *task = fdcs_env_.GetIdleTask();
    if (task) {
      LOG(INFO) << "execute task_id : " << task->task_id();

      ComputingUDF(*task);
      fdcs_env_.TaskCompleted(task->task_id());
    }
//    LOG(INFO) << "IdleTasksSize : " << fdcs_env_.IdleTasksSize();
//    LOG(INFO) << "TasksSize : " << fdcs_env_.TasksSize();
    if (fdcs_env_.TasksSize() == 0)
      HeartBeatSignal();
  }
  LOG(INFO) << "ComputingThread quit!";
}

void Worker::ComputingSignal() {
  for (int i = 0; i < fdcs_env_.ComputingThreads(); ++i) {
    cond_computing_[i].Signal();
  }
}

// Master host connection belongs or Tracker, 
// and did not return to '-1', returns socket_fd
int Worker::FindOwnerMaster() {
  LOG(INFO) << "Master::FindOwnerMaster(" << worker_sockaddr_ << ")";
  const struct node_s *conhash_node = conhash_lookup(master_conhash_, worker_sockaddr_.data());
  if (NULL == conhash_node) return -1;

  string owner_master = conhash_node->iden;// "192.168.1.110:32302";// 
  LOG(INFO) << worker_sockaddr_ << " owner master is " << owner_master;
  if (true == ConnectMaster(owner_master)) {
    return master_sockets_[owner_master]->Socket();
  }
  if (ConnectMaster(primary_sockaddr_)) {
    return master_sockets_[primary_sockaddr_]->Socket();
  }
  return -1;
}

// Find an online Master host, did not return a '-1', returns socket_fd
int Worker::FindMaster() {
  LOG(INFO) << "Master::FindMaster()";

  std::string tracker_group = fdcs_env_.TrackerGroup();
  std::vector<std::string> master_sockaddr;
  SplitStringUsing(tracker_group, ";", &master_sockaddr);
  CHECK_LT(0, master_sockaddr.size());

  for (int i = 0; i < master_sockaddr.size(); i ++) {
    string master_addr = master_sockaddr[i];

    if (true == ConnectMaster(master_addr)) {
      socket_ids_[master_sockets_[master_addr]->Socket()] = master_addr;
      return master_sockets_[master_addr]->Socket();
    }
  }
  return -1;
}
