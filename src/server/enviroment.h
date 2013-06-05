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
#include "src/server/tracker_protocol.pb.h"
#include "src/system/mutex.h"

#ifndef FASTDCS_SERVER_ENVIROMENT_H_
#define FASTDCS_SERVER_ENVIROMENT_H_

using namespace std;

namespace fastdcs {
namespace server {

typedef std::map<string /*task_id*/, FdcsTask* /*id*/>::iterator TaskMapIter;

class Enviroment
{
 public:
  Enviroment();
  ~Enviroment();

  int32 WorkerAmount() { return worker_amount_; }
  void WorkerAmount(int32 amount) { worker_amount_ = amount; }

  void ClearCompletedTasks();
  int32 CompletedTasksSize(){ MutexLocker locker(&mutex_); return completed_tasks_.size(); }
  FdcsTask* CompletedTasks(int index) { return completed_tasks_[index]; }

  FdcsTask* GetIdleTask();
  int32 IdleTasksSize();
  int32 TasksSize() { MutexLocker locker(&mutex_); return tasks_.size(); }
  bool AddTask(FdcsTask task);
  void UpdateTasks(std::vector<FdcsTask> tasts);
  
  // The task is completed, the new task to move to the task to complete the array.
  void TaskCompleted(string task_id);
  FdcsTask* FindTask(string task_id);
  FdcsTask* FindNextTask(TaskMapIter* iter);
  void CopyTasks(std::vector<FdcsTask>* tasks);
  void LeaseTasks(std::vector<FdcsTask>* tasks);
  TaskMapIter TaskIterBegin() { MutexLocker locker(&mutex_); return tasks_.begin(); }

  // FastDCS system time
  int32 NowTime() { 
    struct timeval now;
    gettimeofday(&now, NULL);

    return now.tv_sec;
  };

  void CalcFdcsTimeDiffer(int32 fdcs_time) {
    fdcs_time_differ_ = fdcs_time - NowTime();
    LOG(INFO) << "fdcs_time_differ_ : " << fdcs_time_differ_;
  };
  int32 FdcsTime() { 
    return NowTime() + fdcs_time_differ_;
  };
  void InitFdcsTimeDiffer() { fdcs_time_differ_ = 0; }

  // settings
  void Settings(struct settings_s settings) { settings_ = settings; }
  const char* TrackerGroup() { return settings_.tracker_group; }
  const char* TrackerServer() { return settings_.tracker_server; }
  int NetworkTimeout() { return settings_.network_timeout; }
  int HeartBeatInterval() { return settings_.heart_beat_interval; }
  int PreloadTasks() { return settings_.preload_tasks; }
  int LeaseTimeout() { return settings_.lease_timeout; }
  int ComputingThreads() { return settings_.computing_threads; }
  int TaskDuplicate() { return settings_.task_duplicate; }
  int SocketBuffSize() { return settings_.socket_buff_size; }
  
 private:
  struct settings_s settings_;
  int32 fdcs_time_differ_; // And the primary master server time differential
  int32 worker_amount_;
  std::vector<FdcsTask*> completed_tasks_;
  std::map<string /*task_id*/, FdcsTask* /*id*/> tasks_;
  Mutex mutex_; // Protect all above data and conditions.
};

} // namespace server
} // namespace fastdcs

#endif  // FASTDCS_SERVER_ENVIROMENT_H_
