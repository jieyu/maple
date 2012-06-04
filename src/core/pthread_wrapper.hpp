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

// File: core/pthread_wrapper.hpp - Define pthread function wrappers.
//
// Here are the prototypes of all the pthread functions on my machine
// (x86_64 RHEL5 with glibc-2.5): (* means implemented)
//
// * int pthread_create (pthread_t *__restrict __newthread, __const pthread_attr_t *__restrict __attr, void *(*__start_routine) (void *), void *__restrict __arg) __THROW __nonnull ((1, 3));
//   void pthread_exit (void *__retval) __attribute__ ((__noreturn__));
// * int pthread_join (pthread_t __th, void **__thread_return);
//   int pthread_tryjoin_np (pthread_t __th, void **__thread_return) __THROW;
//   int pthread_timedjoin_np (pthread_t __th, void **__thread_return, __const struct timespec *__abstime);
//   int pthread_detach (pthread_t __th) __THROW;
//   pthread_t pthread_self (void) __THROW __attribute__ ((__const__));
//   int pthread_equal (pthread_t __thread1, pthread_t __thread2) __THROW;
//   int pthread_attr_init (pthread_attr_t *__attr) __THROW __nonnull ((1));
//   int pthread_attr_destroy (pthread_attr_t *__attr) __THROW __nonnull ((1));
//   int pthread_attr_getdetachstate (__const pthread_attr_t *__attr, int *__detachstate) __THROW __nonnull ((1, 2));
//   int pthread_attr_setdetachstate (pthread_attr_t *__attr, int __detachstate) __THROW __nonnull ((1));
//   int pthread_attr_getguardsize (__const pthread_attr_t *__attr, size_t *__guardsize) __THROW __nonnull ((1, 2));
//   int pthread_attr_setguardsize (pthread_attr_t *__attr, size_t __guardsize) __THROW __nonnull ((1));
//   int pthread_attr_getschedparam (__const pthread_attr_t *__restrict __attr, struct sched_param *__restrict __param) __THROW __nonnull ((1, 2));
//   int pthread_attr_setschedparam (pthread_attr_t *__restrict __attr, __const struct sched_param *__restrict __param) __THROW __nonnull ((1, 2));
//   int pthread_attr_getschedpolicy (__const pthread_attr_t *__restrict __attr, int *__restrict __policy) __THROW __nonnull ((1, 2));
//   int pthread_attr_setschedpolicy (pthread_attr_t *__attr, int __policy) __THROW __nonnull ((1));
//   int pthread_attr_getinheritsched (__const pthread_attr_t *__restrict __attr, int *__restrict __inherit) __THROW __nonnull ((1, 2));
//   int pthread_attr_setinheritsched (pthread_attr_t *__attr, int __inherit) __THROW __nonnull ((1));
//   int pthread_attr_getscope (__const pthread_attr_t *__restrict __attr, int *__restrict __scope) __THROW __nonnull ((1, 2));
//   int pthread_attr_setscope (pthread_attr_t *__attr, int __scope) __THROW __nonnull ((1));
//   int pthread_attr_getstackaddr (__const pthread_attr_t *__restrict __attr, void **__restrict __stackaddr) __THROW __nonnull ((1, 2)) __attribute_deprecated__;
//   int pthread_attr_setstackaddr (pthread_attr_t *__attr, void *__stackaddr) __THROW __nonnull ((1)) __attribute_deprecated__;
//   int pthread_attr_getstacksize (__const pthread_attr_t *__restrict __attr, size_t *__restrict __stacksize) __THROW __nonnull ((1, 2));
//   int pthread_attr_setstacksize (pthread_attr_t *__attr, size_t __stacksize) __THROW __nonnull ((1));
//   int pthread_attr_getstack (__const pthread_attr_t *__restrict __attr, void **__restrict __stackaddr, size_t *__restrict __stacksize) __THROW __nonnull ((1, 2, 3));
//   int pthread_attr_setstack (pthread_attr_t *__attr, void *__stackaddr, size_t __stacksize) __THROW __nonnull ((1));
//   int pthread_attr_setaffinity_np (pthread_attr_t *__attr, size_t __cpusetsize, __const cpu_set_t *__cpuset) __THROW __nonnull ((1, 3));
//   int pthread_attr_getaffinity_np (__const pthread_attr_t *__attr, size_t __cpusetsize, cpu_set_t *__cpuset) __THROW __nonnull ((1, 3));
//   int pthread_getattr_np (pthread_t __th, pthread_attr_t *__attr) __THROW __nonnull ((2));
//   int pthread_setschedparam (pthread_t __target_thread, int __policy, __const struct sched_param *__param) __THROW __nonnull ((3));
//   int pthread_getschedparam (pthread_t __target_thread, int *__restrict __policy, struct sched_param *__restrict __param) __THROW __nonnull ((2, 3));
//   int pthread_setschedprio (pthread_t __target_thread, int __prio) __THROW;
//   int pthread_getconcurrency (void) __THROW;
//   int pthread_setconcurrency (int __level) __THROW;
//   int pthread_yield (void) __THROW;
//   int pthread_setaffinity_np (pthread_t __th, size_t __cpusetsize, __const cpu_set_t *__cpuset) __THROW __nonnull ((3));
//   int pthread_getaffinity_np (pthread_t __th, size_t __cpusetsize, cpu_set_t *__cpuset) __THROW __nonnull ((3));
//   int pthread_once (pthread_once_t *__once_control, void (*__init_routine) (void)) __nonnull ((1, 2));
//   int pthread_setcancelstate (int __state, int *__oldstate);
//   int pthread_setcanceltype (int __type, int *__oldtype);
//   int pthread_cancel (pthread_t __th);
//   void pthread_testcancel (void);
//   int pthread_mutex_init (pthread_mutex_t *__mutex, __const pthread_mutexattr_t *__mutexattr) __THROW __nonnull ((1));
//   int pthread_mutex_destroy (pthread_mutex_t *__mutex) __THROW __nonnull ((1));
// * int pthread_mutex_trylock (pthread_mutex_t *__mutex) __THROW __nonnull ((1));
// * int pthread_mutex_lock (pthread_mutex_t *__mutex) __THROW __nonnull ((1));
//   int pthread_mutex_timedlock (pthread_mutex_t *__restrict __mutex, __const struct timespec *__restrict __abstime) __THROW __nonnull ((1, 2));
// * int pthread_mutex_unlock (pthread_mutex_t *__mutex) __THROW __nonnull ((1));
//   int pthread_mutex_getprioceiling (__const pthread_mutex_t * __restrict __mutex, int *__restrict __prioceiling) __THROW __nonnull ((1, 2));
//   int pthread_mutex_setprioceiling (pthread_mutex_t *__restrict __mutex, int __prioceiling, int *__restrict __old_ceiling) __THROW __nonnull ((1, 3));
//   int pthread_mutex_consistent_np (pthread_mutex_t *__mutex) __THROW __nonnull ((1));
//   int pthread_mutexattr_init (pthread_mutexattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_mutexattr_getpshared (__const pthread_mutexattr_t * __restrict __attr, int *__restrict __pshared) __THROW __nonnull ((1, 2));
//   int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared) __THROW __nonnull ((1));
//   int pthread_mutexattr_gettype (__const pthread_mutexattr_t *__restrict __attr, int *__restrict __kind) __THROW __nonnull ((1, 2));
//   int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) __THROW __nonnull ((1));
//   int pthread_mutexattr_getprotocol (__const pthread_mutexattr_t * __restrict __attr, int *__restrict __protocol) __THROW __nonnull ((1, 2));
//   int pthread_mutexattr_setprotocol (pthread_mutexattr_t *__attr, int __protocol) __THROW __nonnull ((1));
//   int pthread_mutexattr_getprioceiling (__const pthread_mutexattr_t * __restrict __attr, int *__restrict __prioceiling) __THROW __nonnull ((1, 2));
//   int pthread_mutexattr_setprioceiling (pthread_mutexattr_t *__attr, int __prioceiling) __THROW __nonnull ((1));
//   int pthread_mutexattr_getrobust_np (__const pthread_mutexattr_t *__attr, int *__robustness) __THROW __nonnull ((1, 2));
//   int pthread_mutexattr_setrobust_np (pthread_mutexattr_t *__attr, int __robustness) __THROW __nonnull ((1));
//   int pthread_rwlock_init (pthread_rwlock_t *__restrict __rwlock, __const pthread_rwlockattr_t *__restrict __attr) __THROW __nonnull ((1));
//   int pthread_rwlock_destroy (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlock_rdlock (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlock_tryrdlock (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlock_timedrdlock (pthread_rwlock_t *__restrict __rwlock, __const struct timespec *__restrict __abstime) __THROW __nonnull ((1, 2));
//   int pthread_rwlock_wrlock (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlock_trywrlock (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlock_timedwrlock (pthread_rwlock_t *__restrict __rwlock, __const struct timespec *__restrict __abstime) __THROW __nonnull ((1, 2));
//   int pthread_rwlock_unlock (pthread_rwlock_t *__rwlock) __THROW __nonnull ((1));
//   int pthread_rwlockattr_init (pthread_rwlockattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_rwlockattr_destroy (pthread_rwlockattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_rwlockattr_getpshared (__const pthread_rwlockattr_t * __restrict __attr, int *__restrict __pshared) __THROW __nonnull ((1, 2));
//   int pthread_rwlockattr_setpshared (pthread_rwlockattr_t *__attr, int __pshared) __THROW __nonnull ((1));
//   int pthread_rwlockattr_getkind_np (__const pthread_rwlockattr_t * __restrict __attr, int *__restrict __pref) __THROW __nonnull ((1, 2));
//   int pthread_rwlockattr_setkind_np (pthread_rwlockattr_t *__attr, int __pref) __THROW __nonnull ((1));
//   int pthread_cond_init (pthread_cond_t *__restrict __cond, __const pthread_condattr_t *__restrict __cond_attr) __THROW __nonnull ((1));
//   int pthread_cond_destroy (pthread_cond_t *__cond) __THROW __nonnull ((1));
// * int pthread_cond_signal (pthread_cond_t *__cond) __THROW __nonnull ((1));
// * int pthread_cond_broadcast (pthread_cond_t *__cond) __THROW __nonnull ((1));
// * int pthread_cond_wait (pthread_cond_t *__restrict __cond, pthread_mutex_t *__restrict __mutex) __nonnull ((1, 2));
// * int pthread_cond_timedwait (pthread_cond_t *__restrict __cond, pthread_mutex_t *__restrict __mutex, __const struct timespec *__restrict __abstime) __nonnull ((1, 2, 3));
//   int pthread_condattr_init (pthread_condattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_condattr_destroy (pthread_condattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_condattr_getpshared (__const pthread_condattr_t * __restrict __attr, int *__restrict __pshared) __THROW __nonnull ((1, 2));
//   int pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared) __THROW __nonnull ((1));
//   int pthread_condattr_getclock (__const pthread_condattr_t * __restrict __attr, __clockid_t *__restrict __clock_id) __THROW __nonnull ((1, 2));
//   int pthread_condattr_setclock (pthread_condattr_t *__attr, __clockid_t __clock_id) __THROW __nonnull ((1));
//   int pthread_spin_init (pthread_spinlock_t *__lock, int __pshared) __THROW __nonnull ((1));
//   int pthread_spin_destroy (pthread_spinlock_t *__lock) __THROW __nonnull ((1));
//   int pthread_spin_lock (pthread_spinlock_t *__lock) __THROW __nonnull ((1));
//   int pthread_spin_trylock (pthread_spinlock_t *__lock) __THROW __nonnull ((1));
//   int pthread_spin_unlock (pthread_spinlock_t *__lock) __THROW __nonnull ((1));
// * int pthread_barrier_init (pthread_barrier_t *__restrict __barrier, __const pthread_barrierattr_t *__restrict __attr, unsigned int __count) __THROW __nonnull ((1));
//   int pthread_barrier_destroy (pthread_barrier_t *__barrier) __THROW __nonnull ((1));
// * int pthread_barrier_wait (pthread_barrier_t *__barrier) __THROW __nonnull ((1));
//   int pthread_barrierattr_init (pthread_barrierattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr) __THROW __nonnull ((1));
//   int pthread_barrierattr_getpshared (__const pthread_barrierattr_t * __restrict __attr, int *__restrict __pshared) __THROW __nonnull ((1, 2));
//   int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr, int __pshared) __THROW __nonnull ((1));
//   int pthread_key_create (pthread_key_t *__key, void (*__destr_function) (void *)) __THROW __nonnull ((1));
//   int pthread_key_delete (pthread_key_t __key) __THROW;
//   void *pthread_getspecific (pthread_key_t __key) __THROW;
//   int pthread_setspecific (pthread_key_t __key, __const void *__pointer) __THROW ;
//   int pthread_getcpuclockid (pthread_t __thread_id, __clockid_t *__clock_id) __THROW __nonnull ((2));
//   int pthread_atfork (void (*__prepare) (void), void (*__parent) (void), void (*__child) (void)) __THROW;

#ifndef CORE_PTHREAD_WRAPPER_HPP_
#define CORE_PTHREAD_WRAPPER_HPP_

#include <pthread.h>

#include "pin.H"

#include "core/basictypes.h"
#include "core/wrapper.hpp"

typedef void *(* PthreadStartRoutineType)(void *);

// Wrapper context for "pthread_create". The original function prototype is:
//
// int pthread_create (pthread_t *restrict thread,
//                     const pthread_attr_t *restrict attr,
//                     void *(*start_routine)(void *),
//                     void *restrict arg);
//
class PthreadCreateContext : public WrapperContext {
 public:
  PthreadCreateContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_t *thread, pthread_attr_t *attr,
                       PthreadStartRoutineType start_routine, void *arg)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        thread_(thread),
        attr_(attr),
        start_routine_(start_routine),
        arg_(arg) {}
  ~PthreadCreateContext() {}

  int ret_val() { return ret_val_; }
  pthread_t *thread() { return thread_; }
  pthread_attr_t *attr() { return attr_; }
  PthreadStartRoutineType start_routine() { return start_routine_; }
  void *arg() { return arg_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_t *thread_;
  pthread_attr_t *attr_;
  PthreadStartRoutineType start_routine_;
  void *arg_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadCreateContext);
};

// Wrapper for "pthread_create".
class PthreadCreateWrapper
    : public Wrapper<PthreadCreateContext, PthreadCreateWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadCreateContext *context);

 private:
  PthreadCreateWrapper() {}
  ~PthreadCreateWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_t *thread, pthread_attr_t *attr,
                       PthreadStartRoutineType start_routine, void *arg);

  DISALLOW_COPY_CONSTRUCTORS(PthreadCreateWrapper);
};

// Wrapper context for "pthread_join". The original function prototype is:
//
// int pthread_join (pthread_t thread, void **value_ptr);
//
class PthreadJoinContext : public WrapperContext {
 public:
  PthreadJoinContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                     pthread_t thread, void **value_ptr)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        thread_(thread),
        value_ptr_(value_ptr) {}
  ~PthreadJoinContext() {}

  int ret_val() { return ret_val_; }
  pthread_t thread() { return thread_; }
  void **value_ptr() { return value_ptr_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_t thread_;
  void **value_ptr_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadJoinContext);
};

// Wrapper for "pthread_join".
class PthreadJoinWrapper
    : public Wrapper<PthreadJoinContext, PthreadJoinWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadJoinContext *context);

 private:
  PthreadJoinWrapper() {}
  ~PthreadJoinWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_t thread, void **value_ptr);

  DISALLOW_COPY_CONSTRUCTORS(PthreadJoinWrapper);
};

// Wrapper context for "pthread_mutex_trylock". The original function
// prototype is:
//
// int pthread_mutex_trylock (pthread_mutex_t *mutex);
//
class PthreadMutexTryLockContext : public WrapperContext {
 public:
  PthreadMutexTryLockContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                             pthread_mutex_t *mutex)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        mutex_(mutex) {}
  ~PthreadMutexTryLockContext() {}

  int ret_val() { return ret_val_; }
  pthread_mutex_t *mutex() { return mutex_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_mutex_t *mutex_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexTryLockContext);
};

// Wrapper for "pthread_mutex_trylock".
class PthreadMutexTryLockWrapper
    : public Wrapper<PthreadMutexTryLockContext, PthreadMutexTryLockWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadMutexTryLockContext *context);

 private:
  PthreadMutexTryLockWrapper() {}
  ~PthreadMutexTryLockWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_mutex_t *mutex);

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexTryLockWrapper);
};

// Wrapper context for "pthread_mutex_lock". The original function
// prototype is:
//
// int pthread_mutex_lock (pthread_mutex_t *mutex);
//
class PthreadMutexLockContext : public WrapperContext {
 public:
  PthreadMutexLockContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                          pthread_mutex_t *mutex)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        mutex_(mutex) {}
  ~PthreadMutexLockContext() {}

  int ret_val() { return ret_val_; }
  pthread_mutex_t *mutex() { return mutex_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_mutex_t *mutex_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexLockContext);
};

// Wrapper for "pthread_mutex_lock".
class PthreadMutexLockWrapper
    : public Wrapper<PthreadMutexLockContext, PthreadMutexLockWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadMutexLockContext *context);

 private:
  PthreadMutexLockWrapper() {}
  ~PthreadMutexLockWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_mutex_t *mutex);

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexLockWrapper);
};

// Wrapper context for "pthread_mutex_unlock". The original function
// prototype is:
//
// int pthread_mutex_unlock (pthread_mutex_t *mutex);
//
class PthreadMutexUnlockContext : public WrapperContext {
 public:
  PthreadMutexUnlockContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                            pthread_mutex_t *mutex)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        mutex_(mutex) {}
  ~PthreadMutexUnlockContext() {}

  int ret_val() { return ret_val_; }
  pthread_mutex_t *mutex() { return mutex_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_mutex_t *mutex_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexUnlockContext);
};

// Wrapper for "pthread_mutex_unlock".
class PthreadMutexUnlockWrapper
    : public Wrapper<PthreadMutexUnlockContext, PthreadMutexUnlockWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadMutexUnlockContext *context);

 private:
  PthreadMutexUnlockWrapper() {}
  ~PthreadMutexUnlockWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_mutex_t *mutex);

  DISALLOW_COPY_CONSTRUCTORS(PthreadMutexUnlockWrapper);
};

// Wrapper context for "pthread_cond_signal". The original function
// prototype is:
//
// int pthread_cond_signal (pthread_cond_t *cond);
//
class PthreadCondSignalContext : public WrapperContext {
 public:
  PthreadCondSignalContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                           pthread_cond_t *cond)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        cond_(cond) {}
  ~PthreadCondSignalContext() {}

  int ret_val() { return ret_val_; }
  pthread_cond_t *cond() { return cond_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_cond_t *cond_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondSignalContext);
};

// Wrapper for "pthread_cond_signal".
class PthreadCondSignalWrapper
    : public Wrapper<PthreadCondSignalContext, PthreadCondSignalWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadCondSignalContext *context);

 private:
  PthreadCondSignalWrapper() {}
  ~PthreadCondSignalWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_cond_t *cond);

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondSignalWrapper);
};

// Wrapper context for "pthread_cond_broadcast". The original function
// prototype is:
//
// int pthread_cond_broadcast (pthread_cond_t *cond);
//
class PthreadCondBroadcastContext : public WrapperContext {
 public:
  PthreadCondBroadcastContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                              pthread_cond_t *cond)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        cond_(cond) {}
  ~PthreadCondBroadcastContext() {}

   int ret_val() { return ret_val_; }
   pthread_cond_t *cond() { return cond_; }
   void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_cond_t *cond_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondBroadcastContext);
};

// Wrapper for "pthread_cond_broadcast".
class PthreadCondBroadcastWrapper
    : public Wrapper<PthreadCondBroadcastContext, PthreadCondBroadcastWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadCondBroadcastContext *context);

 private:
  PthreadCondBroadcastWrapper() {}
  ~PthreadCondBroadcastWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_cond_t *cond);

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondBroadcastWrapper);
};

// Wrapper context for "pthread_cond_wait". The original function
// prototype is:
//
// int pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex);
//
class PthreadCondWaitContext : public WrapperContext {
 public:
  PthreadCondWaitContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         pthread_cond_t *cond, pthread_mutex_t *mutex)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        cond_(cond),
        mutex_(mutex) {}
  ~PthreadCondWaitContext() {}

   int ret_val() { return ret_val_; }
   pthread_cond_t *cond() { return cond_; }
   pthread_mutex_t *mutex() { return mutex_; }
   void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_cond_t *cond_;
  pthread_mutex_t *mutex_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondWaitContext);
};

// Wrapper for "pthread_cond_wait".
class PthreadCondWaitWrapper
    : public Wrapper<PthreadCondWaitContext, PthreadCondWaitWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadCondWaitContext *context);

 private:
  PthreadCondWaitWrapper() {}
  ~PthreadCondWaitWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_cond_t *cond, pthread_mutex_t *mutex);

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondWaitWrapper);
};

// Wrapper context for "pthread_cond_timedwait". The original function
// prototype is:
//
// int pthread_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex,
//                             const struct timespec *abstime);
//
class PthreadCondTimedwaitContext : public WrapperContext {
 public:
  PthreadCondTimedwaitContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                              pthread_cond_t *cond, pthread_mutex_t *mutex,
                              struct timespec *abstime)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        cond_(cond),
        mutex_(mutex),
        abstime_(abstime) {}
  ~PthreadCondTimedwaitContext() {}

  int ret_val() { return ret_val_; }
  pthread_cond_t *cond() { return cond_; }
  pthread_mutex_t *mutex() { return mutex_; }
  struct timespec *abstime() { return abstime_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_cond_t *cond_;
  pthread_mutex_t *mutex_;
  struct timespec *abstime_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondTimedwaitContext);
};

// Wrapper for "pthread_cond_timedwait".
class PthreadCondTimedwaitWrapper
    : public Wrapper<PthreadCondTimedwaitContext, PthreadCondTimedwaitWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadCondTimedwaitContext *context);

 private:
  PthreadCondTimedwaitWrapper() {}
  ~PthreadCondTimedwaitWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_cond_t *cond, pthread_mutex_t *mutex,
                       struct timespec *abstime);

  DISALLOW_COPY_CONSTRUCTORS(PthreadCondTimedwaitWrapper);
};

// Wrapper context for "pthread_barrier_init". The original function
// prototype is:
//
// int pthread_barrier_init (pthread_barrier_t *barrier,
//                           const pthread_barrierattr_t *restrict attr,
//                           unsigned int count);
//
class PthreadBarrierInitContext : public WrapperContext {
 public:
  PthreadBarrierInitContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                            pthread_barrier_t *barrier,
                            pthread_barrierattr_t *attr, unsigned int count)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        barrier_(barrier),
        attr_(attr),
        count_(count) {}
  ~PthreadBarrierInitContext() {}

  int ret_val() { return ret_val_; }
  pthread_barrier_t *barrier() { return barrier_; }
  pthread_barrierattr_t *attr() { return attr_; }
  unsigned int count() { return count_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_barrier_t *barrier_;
  pthread_barrierattr_t *attr_;
  unsigned int count_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadBarrierInitContext);
};

// Wrapper for "pthread_barrier_init".
class PthreadBarrierInitWrapper
    : public Wrapper<PthreadBarrierInitContext, PthreadBarrierInitWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadBarrierInitContext *context);

 private:
  PthreadBarrierInitWrapper() {}
  ~PthreadBarrierInitWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_barrier_t *barrier, pthread_barrierattr_t *attr,
                       unsigned int count);

  DISALLOW_COPY_CONSTRUCTORS(PthreadBarrierInitWrapper);
};

// Wrapper context for "pthread_barrier_wait". The original function
// prototype is:
//
// int pthread_barrier_wait (pthread_barrier_t *barrier);
//
class PthreadBarrierWaitContext : public WrapperContext {
 public:
  PthreadBarrierWaitContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                            pthread_barrier_t *barrier)
      : WrapperContext(tid, ctxt, ret_addr),
        ret_val_(0),
        barrier_(barrier) {}
  ~PthreadBarrierWaitContext() {}

  int ret_val() { return ret_val_; }
  pthread_barrier_t *barrier() { return barrier_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pthread_barrier_t *barrier_;

  DISALLOW_COPY_CONSTRUCTORS(PthreadBarrierWaitContext);
};

// Wrapper for "pthread_barrier_wait".
class PthreadBarrierWaitWrapper
    : public Wrapper<PthreadBarrierWaitContext, PthreadBarrierWaitWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(PthreadBarrierWaitContext *context);

 private:
  PthreadBarrierWaitWrapper() {}
  ~PthreadBarrierWaitWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pthread_barrier_t *barrier);

  DISALLOW_COPY_CONSTRUCTORS(PthreadBarrierWaitWrapper);
};

// Global functions to install pthread wrappers
void register_pthread_wrappers(IMG img);

#endif // end of #ifndef CORE_PTHREAD_WRAPPER_HPP_

