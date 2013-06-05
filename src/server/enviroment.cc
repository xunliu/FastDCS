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
#include <sys/time.h>

#include "src/base/logging.h"
#include "src/utils/stl_util.h"
#include "src/utils/string_codec.h"
#include "src/server/enviroment.h"

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

Enviroment::Enviroment() {
  worker_amount_ = 0;
}

Enviroment::~Enviroment() {
  LOG(INFO) << "Enviroment::~Enviroment()";
  MutexLocker locker(&mutex_);
  STLDeleteElementsAndClear(&completed_tasks_);
  STLDeleteValuesAndClear(&tasks_);
}

bool Enviroment::AddTask(FdcsTask task) {
//  LOG(INFO) << "Enviroment::AddTask(" << task.task_id() << ")";
  MutexLocker locker(&mutex_);
  TaskMapIter task_iter = tasks_.find(task.task_id());
  if (task_iter != tasks_.end() && NULL != task_iter->second) {
    LOG(WARNING) << "Already exists task : " << task.task_id();
    return false;
  }

  FdcsTask *new_task = new FdcsTask();
  new_task->CopyFrom(task);
  tasks_.insert(std::pair<string, FdcsTask*>(new_task->task_id(), new_task));

  return true;
}

void Enviroment::UpdateTasks(std::vector<FdcsTask> tasks) {
  LOG(INFO) << "Enviroment::UpdateTasks(" << tasks.size() << ")";
  MutexLocker locker(&mutex_);
  STLDeleteValuesAndClear(&tasks_);
  ClearCompletedTasks();
  for (int i = 0; i < tasks.size(); ++i) {
    FdcsTask *new_task = new FdcsTask();
    new_task->CopyFrom(tasks[i]);
    tasks_.insert(std::pair<string, FdcsTask*>(new_task->task_id(), new_task));
  }
}

void Enviroment::LeaseTasks(std::vector<FdcsTask>* tasks) {
  int task_duplicate = TaskDuplicate();
  int lease_timeout = LeaseTimeout();
  LOG(INFO) << "Enviroment::LeaseTasks(" << task_duplicate << ", " << lease_timeout << ")";

  MutexLocker locker(&mutex_);
  for (TaskMapIter it = tasks_.begin(); it != tasks_.end(); ++it) {
    if(it->second->lease_time() < FdcsTime()) {
      if (task_duplicate-- <= 0) break;

      FdcsTask new_task;
      new_task.CopyFrom(*it->second);
      // update enviroment tasks and lease tasks lease time.
      it->second->set_lease_time(FdcsTime() + lease_timeout);
      new_task.clear_lease_time();
      tasks->push_back(new_task);
    }
  }
}

void Enviroment::CopyTasks(std::vector<FdcsTask>* tasks) {
//  LOG(INFO) << "Enviroment::CopyTasks()";
  MutexLocker locker(&mutex_);  
  for (TaskMapIter it = tasks_.begin(); it != tasks_.end(); ++it) {
    if(it->second) {
      FdcsTask new_task;
      new_task.CopyFrom(*it->second);
      tasks->push_back(new_task);
    }
  }
}

FdcsTask* Enviroment::FindTask(string task_id) {
  LOG(INFO) << "Enviroment::FindTask(" << task_id << ")";
  MutexLocker locker(&mutex_);
  TaskMapIter task_iter = tasks_.find(task_id);
  if (task_iter != tasks_.end() && NULL != task_iter->second) {
    LOG(INFO) << "found: task, id : " << task_id;
    return task_iter->second;
  }
  return NULL;
}

int32_t Enviroment::IdleTasksSize() {
  int32_t idle_size = 0;

  MutexLocker locker(&mutex_);
  for (TaskMapIter it = tasks_.begin(); it != tasks_.end(); ++it) {
    FdcsTask *task = it->second;
//    CHECK(task);
//    LOG(INFO) << "task->lease_time() = " << task->lease_time();
//    LOG(INFO) << "FdcsTime() = " << FdcsTime();
    if (task->lease_time() < FdcsTime()) {
      ++idle_size;
    }
  }
  return idle_size;
}

FdcsTask* Enviroment::GetIdleTask() {
//  LOG(INFO) << "Enviroment::GetIdleTask()";
  MutexLocker locker(&mutex_);
  for (TaskMapIter it = tasks_.begin(); it != tasks_.end(); ++it) {
    FdcsTask *task = it->second;
    CHECK(task);
    if (task->lease_time() < FdcsTime()) {
        task->set_lease_time(FdcsTime() + LeaseTimeout());
        return task;
    }
  }
  return NULL;
}

FdcsTask* Enviroment::FindNextTask(TaskMapIter* iter) {
  LOG(INFO) << "Enviroment::FindNextTask()";
  MutexLocker locker(&mutex_);
  if ((*iter) != tasks_.end()) {
    FdcsTask* task = (*iter)->second;
//    LOG(INFO) << "found: task, id : " << task->task_id();
    ++(*iter);
    return task;
  }
  return NULL;
}

void Enviroment::TaskCompleted(string task_id) {
  LOG(INFO) << "Enviroment::TaskCompleted(" << task_id << ")";
  MutexLocker locker(&mutex_);

  TaskMapIter it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    tasks_.erase(task_id);
    completed_tasks_.push_back(it->second);
  } else {
    LOG(ERROR) << "not found : task id = " << task_id;
  }
}

void Enviroment::ClearCompletedTasks() {
  LOG(INFO) << "Enviroment::ClearCompletedTasks()";
  MutexLocker locker(&mutex_);
  STLDeleteElementsAndClear(&completed_tasks_);
}
