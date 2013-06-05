/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// This implementation is a Scoped Locker
// 
#ifndef SYSTEM_SCOPED_LOCKER_H_
#define SYSTEM_SCOPED_LOCKER_H_

#include "src/base/common.h"

namespace fastdcs {

template <typename LockType>
class ScopedLocker {
 public:
  explicit ScopedLocker(LockType* lock) : m_lock(lock) {
    m_lock->Lock();
  }
  ~ScopedLocker() {
    m_lock->Unlock();
  }
 private:
  LockType* m_lock;
};

template <typename LockType>
class ScopedReaderLocker {
 public:
  explicit ScopedReaderLocker(LockType* lock) : m_lock(lock) {
    m_lock->ReaderLock();
  }
  ~ScopedReaderLocker() {
    m_lock->ReaderUnlock();
  }
 private:
  LockType* m_lock;
};

template <typename LockType>
class ScopedWriterLocker {
 public:
  explicit ScopedWriterLocker(LockType* lock) : m_lock(*lock) {
    m_lock.WriterLock();
  }
  ~ScopedWriterLocker() {
    m_lock.WriterUnlock();
  }
 private:
  LockType& m_lock;
};

} // namespace fastdcs

#endif  // SYSTEM_SCOPED_LOCKER_H_
