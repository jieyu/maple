// Copyright 2011 The University of Michigan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: core/sync.h - Define synchronizations.

#ifndef CORE_SYNC_H_
#define CORE_SYNC_H_

#include <semaphore.h>

#include "core/basictypes.h"

// Define mutex interface.
class Mutex {
 public:
  Mutex() {}
  virtual ~Mutex() {}

  virtual void Lock() = 0;
  virtual void Unlock() = 0;
  virtual Mutex *Clone() = 0;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Mutex);
};

// Define read-write mutex interface.
class RWMutex {
 public:
  RWMutex() {}
  virtual ~RWMutex() {}

  virtual void LockRead() = 0;
  virtual void UnlockRead() = 0;
  virtual void LockWrite() = 0;
  virtual void UnlockWrite() = 0;
  virtual RWMutex *Clone() = 0;

 private:
  DISALLOW_COPY_CONSTRUCTORS(RWMutex);
};

// Define semaphore interface.
class Semaphore {
 public:
  Semaphore() {}
  virtual ~Semaphore() {}

  virtual int Init(unsigned int value) = 0;
  virtual int Wait() = 0;
  virtual int TimedWait(const struct timespec *to) = 0;
  virtual int Post() = 0;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Semaphore);
};

// Define the null mutex (used in single threaded mode)
class NullMutex : public Mutex {
 public:
  NullMutex() {}
  ~NullMutex() {}

  void Lock() {}
  void Unlock() {}
  Mutex *Clone() { return new NullMutex; }

 private:
  DISALLOW_COPY_CONSTRUCTORS(NullMutex);
};

// Define the null read-write mutex (used in single threaded mode)
class NullRWMutex : public RWMutex {
 public:
  NullRWMutex() {}
  ~NullRWMutex() {}

  void LockRead() {}
  void UnlockRead() {}
  void LockWrite() {}
  void UnlockWrite() {}
  RWMutex *Clone() { return new NullRWMutex; }

 private:
  DISALLOW_COPY_CONSTRUCTORS(NullRWMutex);
};

// Define the semaphore implemented by the underlying os.
class SysSemaphore : public Semaphore {
 public:
  SysSemaphore() {}
  explicit SysSemaphore(unsigned int value) { Init(value); }
  ~SysSemaphore() { sem_destroy(&sem_); }

  int Init(unsigned int value) { return sem_init(&sem_, 0, value); }
  int Wait() { return sem_wait(&sem_); }
  int TimedWait(const struct timespec *to) { return sem_timedwait(&sem_, to); }
  int Post() { return sem_post(&sem_); }

 private:
  sem_t sem_;

  DISALLOW_COPY_CONSTRUCTORS(SysSemaphore);
};

// Define scoped lock.
class ScopedLock {
 public:
  explicit ScopedLock(Mutex *mutex)
      : mutex_(mutex), locked_(false) {
    mutex_->Lock();
    locked_ = true;
  }

  ScopedLock(Mutex *mutex, bool initially_locked)
      : mutex_(mutex), locked_(false) {
    if (initially_locked) {
      mutex_->Lock();
      locked_ = true;
    }
  }

  ~ScopedLock() {
    if (locked_)
      mutex_->Unlock();
  }

 private:
  Mutex *mutex_;
  bool locked_;

  DISALLOW_COPY_CONSTRUCTORS(ScopedLock);
};

#endif

