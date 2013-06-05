/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// This implementation is a condition variable
// 
#ifndef SYSTEM_CONDITION_VARIABLE_H_
#define SYSTEM_CONDITION_VARIABLE_H_

#ifndef _WIN32
#if defined __unix__ || defined __APPLE__
#include <pthread.h>
#endif
#endif

#include <assert.h>
#include "src/system/mutex.h"

namespace fastdcs {

class ConditionVariable {
 public:
  ConditionVariable();
  ~ConditionVariable();

  void Signal();
  void Broadcast();

  bool Wait(Mutex* inMutex, int inTimeoutInMilSecs);
  void Wait(Mutex* inMutex);

 private:
#if defined _WIN32
  HANDLE m_hCondition;
  unsigned int m_nWaitCount;
#elif defined __unix__ || defined __APPLE__
  pthread_cond_t m_hCondition;
#endif
  static void CheckError(const char* context, int error);
};

}// namespace fastdcs

#endif  // SYSTEM_CONDITION_VARIABLE_H_
