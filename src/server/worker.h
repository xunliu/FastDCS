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
#include "src/utils/split_string.h"
#include "tracker.h"

#ifndef FASTDCS_SERVER_WORKER_H_
#define FASTDCS_SERVER_WORKER_H_

namespace fastdcs {
namespace server {

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

class Worker;
typedef struct thread_index_s{
  Worker* worker_;
  int index_;
  thread_index_s(){}
  thread_index_s(Worker* worker, int index) {
    worker_ = worker;
    index_ = index;
  }
} thread_index_t;

class Worker : public Tracker {
 protected:
  /*virtual*/ void InitialTracker(struct settings_s settings);
  /*virtual*/ void FinalizeTracker();
  /*virtual*/ bool ProtocolProcess(int socket_fd, TrackerProtocol tracker_proto);
  /*virtual*/ bool HeartBeat();
  /*virtual*/ bool AskTracker();

 private:
  bool RequestTask();
  bool SaveNewTask(TrackerProtocol tracker_proto);

  // Find an online Master host, did not return a '-1', returns socket_fd
  int FindMaster();
  // Master host connection belongs or Tracker, and did not return to '-1', 
  // returns socket_fd
  int FindOwnerMaster();

  // User-defined computing functions
  virtual bool ComputingUDF(FdcsTask &task) { return false; };
  
  static void* CallComputing(void *pvoid);
  void Computing(void *pvoid);
  void ComputingSignal();
  pthread_t *computing_tid_;  // Computing of thread
  ConditionVariable *cond_computing_;  // Data exchange for variables
  Mutex *mutex_computing_; // Protect all above data and conditions.

  struct thread_index_s *thread_index_;

};

} // namespace server
} // namespace fastdcs

#endif  // FASTDCS_SERVER_WORKER_H_
