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
#include "src/base/common_define.h"
#include "src/utils/split_string.h"
#include "src/utils/string_codec.h"
#include "src/server/protofile.h"
#include "src/server/master.h"

REGISTER_FASTDCS_TRACKER(Master);

/*static*/
void Master::Initialize(struct settings_s settings) {
  Tracker::Initialize(settings);
}

/*static*/
void Master::Finalize() {
  Tracker::Finalize();
}

//-----------------------------------------------------------------------------
// Implementation of Master
//-----------------------------------------------------------------------------
/*virtual*/
void Master::InitialTracker(struct settings_s settings) {
  LOG(INFO) << "Master::InitialMaster()";
  Tracker::InitialTracker(settings);

  is_master_ = true;

  CHECK_EQ(pthread_create(&task_exchange_tid_, NULL, CallTaskExchange, (void*)this), 0);

  // Established under tracker_server socket listen
  std::string tracker_server = settings.tracker_server;

  // Establish a Master Tracker monitor
  TCPSocket *socket = new TCPSocket();
  vector<string> ip_and_port;
  SplitStringUsing(tracker_server, ":", &ip_and_port);
  CHECK_EQ(2, ip_and_port.size());
  CHECK_EQ(true, socket->Bind(ip_and_port[0].c_str(), atoi(ip_and_port[1].c_str())));
  CHECK_EQ(true, socket->Listen(1024));
  LOG(INFO) << "create master socket_fd(" << socket->Socket() << ")" << socket->Address();
  socket->SetBlocking(true);

  master_sockets_[tracker_server] = socket;
  AddSocketResource(socket->Socket(), tracker_server);
// UpdateMasterConhash();

  // start the event loop
  int reuseaddr_on = 1;
  setsockopt(socket->Socket(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on));

  poll_event_element_t *p = NULL;
  // add listenfd to poll event
  epoll_->AddToPollEvent(socket->Socket(), EPOLLIN, &p);
  // set callbacks
  p->accept_callback = EpollAccentCallback;
  p->close_callback = EpollCloseCallback;
  // enable accept callback
  p->cb_flags |= ACCEPT_CB;

  // start the event loop
  epoll_->PollEventLoop(); 
}

/*virtual*/ 
void Master::FinalizeTracker() {
  LOG(INFO) << "Master::FinalizeTracker()";

  MasterSignal();
  TaskExchangeSignal();

  void *ret = NULL;
  pthread_join(task_exchange_tid_, &ret);

  return Tracker::FinalizeTracker();
}

/*static*/
void* Master::CallTaskExchange(void *pvoid) {
  ((Master *)pvoid)->TaskExchangeThread(pvoid);
  return NULL;
}

void Master::TaskExchangeThread(void *pvoid) {
  while (!daemon_quit_) {
    MutexLocker locker(&mutex_);
    while (!IAmPrimaryMaster()) {
      if (daemon_quit_) return;
      LOG(INFO) << "cond_task_exchange_.Wait('IAmMaster');";
      cond_master_tracker_.Wait(&mutex_);
    }
    while (fdcs_env_.IdleTasksSize() >= fdcs_env_.PreloadTasks()) {
      if (daemon_quit_) return;
      LOG(INFO) << "fdcs_env_.IdleTasksSize() = " << fdcs_env_.IdleTasksSize();
      cond_task_exchange_.Wait(&mutex_);
    }

    // Output the data that has been processed
    std::vector<FdcsTask> tasks;
    for (int i = 0; i < fdcs_env_.CompletedTasksSize(); ++i) {
      FdcsTask *task = fdcs_env_.CompletedTasks(i);
      FdcsTask copy_task;
      copy_task.CopyFrom(*task);
      tasks.push_back(copy_task);
    }
    ExportTaskUDF(tasks);
    fdcs_env_.ClearCompletedTasks();

    // Request of data waiting to be processed
    LOG(INFO) << "fdcs_env_.IdleTasksSize() = " << fdcs_env_.IdleTasksSize();
    if (fdcs_env_.IdleTasksSize() < fdcs_env_.PreloadTasks()) {
      tasks.clear();
      ImportTaskUDF(tasks);
      for (int i = 0; i < tasks.size(); ++i) {
        fdcs_env_.AddTask(tasks[i]);
      }
    }
    sleep(3);
  }
}

void Master::PrintEnv() {
  LOG(INFO) << "master tracker enviroment";
//  LOG(INFO) << "env_name = " << fdcs_env_.EnvName();
  LOG(INFO) << "CompletedTasksSize = " << fdcs_env_.CompletedTasksSize();
  LOG(INFO) << "IdleTasksSize = " << fdcs_env_.IdleTasksSize();

  LOG(INFO) << "-----CompletedTasks-----";
  for (int i = 0; i < fdcs_env_.CompletedTasksSize(); i ++) {
    LOG(INFO) << "task_id = " << fdcs_env_.CompletedTasks(i)->task_id();
  }
  LOG(INFO) << "-----NewTasks-----";
  vector<FdcsTask> tasks;
  fdcs_env_.CopyTasks(&tasks);
  int tasks_size = tasks.size();
  for (int i = 0; i < tasks_size; ++i) {
    LOG(INFO) << "task_id:" << tasks[i].task_id();
  }
}

bool Master::UpgradeTracker() {
  LOG(INFO) << "Master::UpgradeTracker()";

  fdcs_env_.InitFdcsTimeDiffer();

  MasterSignal();

  return true;
}

/*virtual*/ 
bool Master::HeartBeat() {
//  LOG(INFO) << "Master::HeartBeat()";

  if (IKnowPrimaryMaster()) {
    // 询问Master服务器状态
    if (!IAmPrimaryMaster()) {
      RequestTaskDuplicate();
    }
  } else {
    AskTracker();
  }

  return true;
}

/*virtual*/ 
bool Master::ProtocolProcess(int socket_fd, TrackerProtocol tracker_proto) {
	LOG(INFO) << "Master::ProtocolProcess(" << tracker_proto.protocol_type() << ")";

  string request_socket_addr = tracker_proto.request_sock_addr();
  string response_socket_addr = tracker_proto.response_sock_addr();
  string response_result = tracker_proto.response_result();
  string request_args = tracker_proto.request_args();
  LOG(INFO) << "protocol id = " << tracker_proto.id() 
            << " request_socket_addr = " << request_socket_addr 
            << " ,request_args = " << request_args
            << " ,response_socket_addr = " << response_socket_addr
            << " ,response_result = " << response_result;
  bool closed_circle = ClosedCircle(tracker_proto);

  // Begin communication protocol processing
  switch(tracker_proto.protocol_type()) {
  	case ELECTION: {
  		LOG(INFO) << "[ELECTION] protocol ...";
  		if (closed_circle) {
  			// Election round back to the originator, the last master as a tracker server, 
        // announcement of the results
  			string pass_master = response_socket_addr + ";" + fdcs_env_.TrackerServer();
		  	vector<string> master_addr;
		    SplitStringUsing(fdcs_env_.TrackerGroup(), ";", &master_addr);
		    for (int i = master_addr.size()-1; i >= 0; --i) {
		    	string tracker_addr = master_addr[i];
		    	if (std::string::npos != pass_master.find(tracker_addr)) {
		    		LOG(INFO) << "ELECTION result : " << tracker_addr;
		    		Coordinator(tracker_addr);
		    		break;
		    	}
		    }
  		} else {
  			ContinueTransferProto(tracker_proto);
  		}
    }  
  	break;
  	case COORDINATOR: {
  		LOG(INFO) << "[COORDINATOR] protocol ...";
  		CheckIP(response_result);
  		primary_sockaddr_ = response_result;
  		if (closed_circle) {
  			// Announces election results turn to return to the initiator
  			LOG(INFO) << "[COORDINATOR] new primary master : " << response_result;
//  			ConnectMaster(primary_sockaddr_);
        if (0 == primary_sockaddr_.compare(fdcs_env_.TrackerServer())) 
          UpgradeTracker();
  		} else {
        JoinMasterGroup(tracker_proto);
  			// Continue to pass protocols
  			ContinueTransferProto(tracker_proto);
  		}
    }
  	break;	  		
  	case MASTER_ASK_TRACKER: {
  		LOG(INFO) << "[ASK_TRACTER] protocol ...";
      if (IAmPrimaryMaster()) {
        AddProtoFdcsTime(tracker_proto);
      }
  		if (closed_circle) {
  			// Circle sponsors
  			if (response_result.empty()) {
  				return Election();
  			} else {
  				CheckIP(response_result);
  				primary_sockaddr_ = response_result;
  				LOG(INFO) << "[ASK_TRACTER] around circle request : " 
                    << fdcs_env_.TrackerServer() 
                    << ", primary master : " << response_result;
  				ConnectMaster(primary_sockaddr_);
          JoinMasterGroup(tracker_proto);
  			}
  		} else {
				if (response_result.empty()) {
					if (!primary_sockaddr_.empty()) {
						tracker_proto.set_response_result(primary_sockaddr_);
					}
				} else {
					if (0 != primary_sockaddr_.compare(response_result)
						&& !primary_sockaddr_.empty()) {
						// The notice and ASK_TRACTER Tracker Server conflict
						LOG(ERROR) << "I record primary master is " << primary_sockaddr_ 
                       << ", [ASK_TRACTER] notice primary master is " 
                       << response_result << " conflict!";
						return Election();
					}
				}
	  		ContinueTransferProto(tracker_proto);
  		}
    }
  	break;
  	case MASTER_REPORT_STATUS: {
  		LOG(INFO) << "[MASTER_REPORT_STATUS] protocol ...";
      if (IAmPrimaryMaster()) {
        if (tracker_proto.tracker_status_size() > 0) {
          tracker_status_[fdcs_env_.TrackerServer()] = tracker_proto.tracker_status(0);
          LOG(INFO) << "free_memory = " << tracker_status_[fdcs_env_.TrackerServer()].free_memory();
        }
      } else {
        if (0 == response_result.compare(RESPONSE_RESULT_SUCCESS)) {
          // todo tracker status is ok
        }
      }
    }
		break;
  	case WORKER_ASK_TRACKER:
  		LOG(INFO) << "[WORKER_ASK_TRACKER] protocol ...";
      if (false == primary_sockaddr_.empty()) {
        tracker_proto.set_response_result(primary_sockaddr_);
        AddProtoFdcsTime(tracker_proto);
        socket_ids_[socket_fd] = request_socket_addr;
        SendProtocol(tracker_proto, socket_fd);        
      }
  		break;
    case SYNC_TASK_DUPLICATE: {
      LOG(INFO) << "[SYNC_TASK_DUPLICATE] protocol ...";
      if (!IAmPrimaryMaster()) {
        SaveTaskDuplicate(tracker_proto);
      }
    }
    break;
    case REQUEST_TASK_DUPLICATE: {
      LOG(INFO) << "[REQUEST_TASK_DUPLICATE] protocol ...";
      if (IAmPrimaryMaster()) {
        SendTaskDuplicate(tracker_proto, socket_fd);
      } else {
        SaveTaskDuplicate(tracker_proto);
      }
    }
    break;
    case MASTER_JOIN_GROUP: {
      LOG(INFO) << "[MASTER_JOIN_GROUP] protocol ...";
      ConnectMaster(request_socket_addr);
//      UpdateMasterConhash();
      MasterGroupChanged();
    }
    break;
    case MASTER_GROUP_CHANGED: {
      LOG(INFO) << "[MASTER_GROUP_CHANGED] protocol ...";
      if (closed_circle) {
        LOG(INFO) << "[MASTER_GROUP_CHANGED] protocol completed.";
      } else {
        ConnectOnlineMaster(request_args);
//        UpdateMasterConhash();
        ContinueTransferProto(tracker_proto);
      }
    }
    break;
    case ASK_TASK_STATUS: {
      LOG(INFO) << "[ASK_TASK_STATUS] protocol ...";
      GetTaskStatus(tracker_proto);
      SendProtocol(tracker_proto, socket_fd);
    }
    break;
    case WORKER_REQUEST_TASK: {
      LOG(INFO) << "[WORKER_REQUEST_TASK] protocol ...";
      if (IAmPrimaryMaster()) {
        LOG(INFO) << "Primary Master save complete task ...";
        if (true == TaskCompleted(tracker_proto)) {
          tracker_proto.set_response_result(RESPONSE_RESULT_SUCCESS);
        } else {
          tracker_proto.set_response_result(RESPONSE_RESULT_COMPLETE);
        }
        if (true == GetIdleTask(tracker_proto)) {
          tracker_proto.set_response_result(RESPONSE_RESULT_SUCCESS);
        } else {
          LOG(WARNING) << "GetIdleTask is null!";
          tracker_proto.set_response_result(RESPONSE_RESULT_COMPLETE);
        }
        SendProtocol(tracker_proto, socket_fd);
      } else {
        if (std::string::npos == response_socket_addr.find(fdcs_env_.TrackerServer())) {
          if (IKnowPrimaryMaster()) {
            LOG(INFO) << "Forwarding to primary master processing ...";
            tracker_proto.set_forwarding_sock_fd(socket_fd);
            SendProtocol(tracker_proto, master_sockets_[primary_sockaddr_]->Socket());
          }
        } else {
          LOG(INFO) << "Secondary master then processing ...";
          if (0 == response_result.compare(RESPONSE_RESULT_SUCCESS)) {
            if (true == TaskCompleted(tracker_proto)) {
              tracker_proto.set_response_result(RESPONSE_RESULT_SUCCESS);
            } else {
              tracker_proto.set_response_result(RESPONSE_RESULT_COMPLETE);
            }
          }
          if (true == GetIdleTask(tracker_proto)) {
            tracker_proto.set_response_result(RESPONSE_RESULT_SUCCESS);
          } else {
            LOG(WARNING) << "GetIdleTask is null!";
            tracker_proto.set_response_result(RESPONSE_RESULT_COMPLETE);
            HeartBeatSignal();
          }
          SendProtocol(tracker_proto, tracker_proto.forwarding_sock_fd());  
        }
      }
      TaskExchangeSignal();
    }
    break;   
  	default:
  		LOG(ERROR) << "undefine protocol type:" << tracker_proto.protocol_type();
  	break;
  }

	return true;
}

bool Master::GetIdleTask(TrackerProtocol &tracker_proto) {
  LOG(INFO) << "Master::GetIdleTask(" << tracker_proto.forwarding_sock_fd() << ")";

  if ((tracker_proto.forwarding_sock_fd() > 0) && IAmPrimaryMaster()) {
    LOG(INFO) << "Transmitted by the secondary master, primary master don't process.";
    return true;
  }

  int task_duplicate = KeyToInt32(tracker_proto.request_args());
  tracker_proto.clear_key_values_pairs();
  KeyValuesPair *key_values_pair = tracker_proto.add_key_values_pairs();
  key_values_pair->set_key(KEYVALUEPAIRS_TASK);

  long fdcs_time = fdcs_env_.FdcsTime();
  string task_key;

  TaskMapIter iter = fdcs_env_.TaskIterBegin();
  FdcsTask *env_task = fdcs_env_.FindNextTask(&iter);
  while (env_task) {
    task_key = env_task->task_id();

//    LOG(INFO) << "task_key = " << task_key;
//    LOG(INFO) << "conhash_master = " << conhash_master;
//    LOG(INFO) << "lease_time() = " << env_task->lease_time();
//    LOG(INFO) << "fdcs_time = " << fdcs_time;
//    LOG(INFO) << "LeaseTimeout = " << (env_task->lease_time() - fdcs_time);

    if (env_task->lease_time() < fdcs_time) {
      FdcsTask new_task;
      new_task.CopyFrom(*env_task);
      // update enviroment tasks and lease tasks lease time.
      env_task->set_lease_time(fdcs_time + fdcs_env_.LeaseTimeout());
      new_task.clear_lease_time();

      WriteRecord(key_values_pair, new_task);

      LOG(INFO) << "lease_time() = " << env_task->lease_time();
      LOG(INFO) << "FdcsTime() = " << fdcs_time;
      LOG(INFO) << "New task, task_id = " << env_task->task_id();
      if (--task_duplicate <= 0)
        return true;
    }
    env_task = fdcs_env_.FindNextTask(&iter);
  }
  return false;
}

bool Master::TaskCompleted(TrackerProtocol tracker_proto) {
  LOG(INFO) << "Master::TaskCompleted()";

  std::vector<FdcsTask> completed_tasks;
  for (int i = 0; i < tracker_proto.key_values_pairs_size(); ++i) {
    KeyValuesPair key_values_pair = tracker_proto.key_values_pairs(i);
    if (0 == key_values_pair.key().compare(KEYVALUEPAIRS_TASK_COMPLETE)) {
      ReadRecord(&key_values_pair, &completed_tasks);
    }
  }

  if (completed_tasks.size() == 0) {
    LOG(WARNING) << "Completed task size = " << completed_tasks.size() << "!";
    return false;
  }

  for (int index = 0; index < completed_tasks.size(); ++index) {
    string task_id = completed_tasks[index].task_id();
    FdcsTask *env_task = fdcs_env_.FindTask(task_id);
    if (NULL == env_task) {
      LOG(ERROR) << "Can't find this new task : " << task_id << "!";
      continue;
    }

    LOG(INFO) << "task_id : " << task_id << " env_task.lease_time() = " 
                 << env_task->lease_time() << ", FdcsTime() = " 
                 << fdcs_env_.FdcsTime();
    if (env_task->lease_time() < fdcs_env_.FdcsTime()) {
      LOG(WARNING) << "task_id : " << task_id << " lease timeout! "
                   << "env_task.lease_time() = " 
                   << env_task->lease_time() << ", FdcsTime() = " 
                   << fdcs_env_.FdcsTime();
      continue;
    }
    env_task->CopyFrom(completed_tasks[index]);
    env_task->clear_lease_time();

    fdcs_env_.TaskCompleted(task_id);

    LOG(INFO) << "Task Completed, task_id = " << task_id;
  }
  return true;
}

bool Master::RequestTaskDuplicate() {
  if (IAmPrimaryMaster() || !IKnowPrimaryMaster() || fdcs_env_.IdleTasksSize() > 0) 
    return false;

  LOG(INFO) << "Master::RequestTaskDuplicate()";

  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(REQUEST_TASK_DUPLICATE);

  return SendProtocol(tracker_proto, master_sockets_[primary_sockaddr_]->Socket());
}

bool Master::SendTaskDuplicate(TrackerProtocol tracker_proto, int receiver_fd) {
  if (!IAmPrimaryMaster()) return false;
  LOG(INFO) << "Master::SendTaskDuplicate()";

  if (fdcs_env_.IdleTasksSize() > 0){
    LOG(INFO) << "IdleTasksSize : " << fdcs_env_.IdleTasksSize();
    tracker_proto.clear_key_values_pairs();
    KeyValuesPair *key_values_pair = tracker_proto.add_key_values_pairs();
    key_values_pair->set_key(KEYVALUEPAIRS_TASK);
    
    vector<FdcsTask> tasks;
    fdcs_env_.LeaseTasks(&tasks);
    int tasks_size = tasks.size();
    if (tasks_size > 0) {
      for (int i = 0; i < tasks_size; ++i) {
        WriteRecord(key_values_pair, tasks[i]);
      }
      SendProtocol(tracker_proto, receiver_fd);
    }
  }
  return true;
}

bool Master::SaveTaskDuplicate(TrackerProtocol tracker_proto) {
  LOG(INFO) << "Master::SaveDuplicate()";

  for (int i = 0; i < tracker_proto.key_values_pairs_size(); ++i) {
    KeyValuesPair key_values_pairs = tracker_proto.key_values_pairs(i);
    if (0 != key_values_pairs.key().compare(KEYVALUEPAIRS_TASK))
      continue;

    std::vector<FdcsTask> tasts;
    ReadRecord(&key_values_pairs, &tasts);
    fdcs_env_.UpdateTasks(tasts);
  }
  PrintEnv();
  return true;
}

// 选举Master Tracker服务器
bool Master::Election() {
  LOG(INFO) << "Master::Election()";
  int next_master_fd = FindNextMaster();
  if (next_master_fd == -1) {
    // There is no next Master host, 
    // go directly to the announcement of election results
    primary_sockaddr_ = fdcs_env_.TrackerServer();
    LOG(INFO) << "Only one Master host, primary master : " << primary_sockaddr_;
    UpgradeTracker();
    return true;
  }
  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(ELECTION);

  return SendProtocol(tracker_proto, next_master_fd);
}

// Announcement of election results
bool Master::Coordinator(string tracker_addr) {
  LOG(INFO) << "Master::Coordinator()";
  int next_master_fd = FindNextMaster();
  if (next_master_fd == -1) {
    primary_sockaddr_ = fdcs_env_.TrackerServer();
    LOG(INFO) << "Only one Master host, primary master : " << primary_sockaddr_;
    UpgradeTracker();
    return true;
  }
  TrackerProtocol tracker_proto;
  tracker_proto = NewTrackerProtocol(COORDINATOR);
  tracker_proto.set_response_result(tracker_addr); // Election result
  AddProtoFdcsTime(tracker_proto);

  return SendProtocol(tracker_proto, next_master_fd);
}

// add Fdcs_time to TrackerProtocol
void Master::AddProtoFdcsTime(TrackerProtocol &tracker_proto) {
  string fdcs_time = Int32ToKey(fdcs_env_.FdcsTime());
  LOG(INFO) << " Master::AddProtoFdcsTime(" << fdcs_time << ")";

  KeyValuesPair *key_values_pair = tracker_proto.add_key_values_pairs();
  key_values_pair->set_key(KEYVALUEPAIRS_FDCS_TIME);
  KeyValuePair key_value_pair;
  key_value_pair.set_key(KEYVALUEPAIRS_FDCS_TIME);
  key_value_pair.set_value(fdcs_time);
  WriteRecord(key_values_pair, key_value_pair);
}

// New Master services Master services group
bool Master::JoinMasterGroup(TrackerProtocol tracker_proto) {
  LOG(INFO) << "Master::JoinMasterGroup()";
  if (!IKnowPrimaryMaster() 
    || (0 == primary_sockaddr_.compare(fdcs_env_.TrackerServer())))
    return false;

  if (false == ConnectMaster(primary_sockaddr_)) {
    LOG(WARNING) << "ConnectMaster : " << primary_sockaddr_ << " faildure!";
    return false;
  }

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

  TrackerProtocol new_tracker_proto;
  new_tracker_proto = NewTrackerProtocol(MASTER_JOIN_GROUP);

  return SendProtocol(new_tracker_proto, master_sockets_[primary_sockaddr_]->Socket());
}

void Master::ConnectOnlineMaster(string all_master) {
  LOG(INFO) << "Master::ConnectAllMaster(" << all_master << ")";

  vector<string> master_addr;
  SplitStringUsing(all_master, ";", &master_addr);
  int master_count = master_addr.size();

  for (int index = 0; index < master_count; ++index) {
    string master_address = master_addr[index];
    ConnectMaster(master_address);
  }
}

// Continue to pass communications protocol
void Master::ContinueTransferProto(fastdcs::TrackerProtocol tracker_proto) {
  LOG(INFO) << "Master::ContinueTransferProto(" << tracker_proto.protocol_type() << ")";

  int next_master_fd = FindNextMaster();
  if (-1 != next_master_fd) {
    SendProtocol(tracker_proto, next_master_fd);
  } else {
    LOG(WARNING) << "Not found under the Master server!";
  }
}

/*virtual*/
bool Master::AskTracker() {
  LOG(INFO) << "Master::AskTracker()";

  // I don't know who is the primary Master Server, asking who is a Tracker Server?
  int next_master_fd = FindNextMaster();
  if (-1 == next_master_fd) {
    // Currently there is only one Master Server
    LOG(INFO) << "Only one Master host : " << fdcs_env_.TrackerServer();
    return Election();
  } else {
    TrackerProtocol tracker_proto;
    tracker_proto = NewTrackerProtocol(MASTER_ASK_TRACKER);
    return SendProtocol(tracker_proto, next_master_fd);
  }
}

// Ask Tracker server status
bool Master::MasterReportStatus() {
  if (ConnectMaster(primary_sockaddr_)) {
	  TrackerStatus();
	  TrackerProtocol tracker_proto;
	  tracker_proto = NewTrackerProtocol(MASTER_REPORT_STATUS);  
	  fastdcs::TrackerStatus *tracker_status;
	  tracker_status = tracker_proto.add_tracker_status();
	  tracker_status->CopyFrom(tracker_status_[fdcs_env_.TrackerServer()]);
	  return SendProtocol(tracker_proto, master_sockets_[primary_sockaddr_]->Socket());
  }
  return false;
}

// Closed the circle
bool Master::ClosedCircle(TrackerProtocol tracker_proto) {
  string request_socket_addr = tracker_proto.request_sock_addr();
  string response_socket_addr = tracker_proto.response_sock_addr();

  LOG(INFO) << "request_socket_addr=" << request_socket_addr 
            << ",response_socket_addr=" << response_socket_addr;
  if (0 == request_socket_addr.compare(fdcs_env_.TrackerServer())) {
  	LOG(INFO) << "Closed Circle!";
  	return true; // Circle sponsors
  }
	string pass_socket_addr = request_socket_addr + ";" + response_socket_addr;
  if (std::string::npos != response_socket_addr.find(fdcs_env_.TrackerServer())) {
  	LOG(INFO) << "More than a starting point!";
  	return true; // Sponsors may have exceptions, has been launched in more than
  }

  LOG(INFO) << "Continue transfer.";
  return false; // Has not returned to the originator
}
