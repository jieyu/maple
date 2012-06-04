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

// File: core/pthread_wrapper.cpp - Pthread function wrapper implementation.

#include "core/pthread_wrapper.hpp"

#include "core/logging.h"

void PthreadCreateWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_t *),
                                 PIN_PARG(pthread_attr_t *),
                                 PIN_PARG(PthreadStartRoutineType),
                                 PIN_PARG(void *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                         IARG_END);
  }
}

void PthreadCreateWrapper::CallOriginal(PthreadCreateContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_t *), context->thread(),
      PIN_PARG(pthread_attr_t *), context->attr(),
      PIN_PARG(PthreadStartRoutineType), context->start_routine(),
      PIN_PARG(void *), context->arg(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadCreateWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                    ADDRINT ret_addr, pthread_t *thread,
                                    pthread_attr_t *attr,
                                    PthreadStartRoutineType start_routine,
                                    void *arg) {
  PthreadCreateContext context(tid, ctxt, ret_addr, thread, attr,
                               start_routine, arg);
  handler_(&context);
  return context.ret_val();
}

void PthreadJoinWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_t),
                                 PIN_PARG(void **),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_END);
  }
}

void PthreadJoinWrapper::CallOriginal(PthreadJoinContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_t), context->thread(),
      PIN_PARG(void **), context->value_ptr(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadJoinWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                  ADDRINT ret_addr, pthread_t thread,
                                  void **value_ptr) {
  PthreadJoinContext context(tid, ctxt, ret_addr, thread, value_ptr);
  handler_(&context);
  return context.ret_val();
}

void PthreadMutexTryLockWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_mutex_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadMutexTryLockWrapper::CallOriginal(
    PthreadMutexTryLockContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_mutex_t *), context->mutex(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadMutexTryLockWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                          ADDRINT ret_addr,
                                          pthread_mutex_t *mutex) {
  PthreadMutexTryLockContext context(tid, ctxt, ret_addr, mutex);
  handler_(&context);
  return context.ret_val();
}

void PthreadMutexLockWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_mutex_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadMutexLockWrapper::CallOriginal(PthreadMutexLockContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_mutex_t *), context->mutex(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadMutexLockWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                       ADDRINT ret_addr,
                                       pthread_mutex_t *mutex) {
  PthreadMutexLockContext context(tid, ctxt, ret_addr, mutex);
  handler_(&context);
  return context.ret_val();
}

void PthreadMutexUnlockWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_mutex_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadMutexUnlockWrapper::CallOriginal(
    PthreadMutexUnlockContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_mutex_t *), context->mutex(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadMutexUnlockWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                         ADDRINT ret_addr,
                                         pthread_mutex_t *mutex) {
  PthreadMutexUnlockContext context(tid, ctxt, ret_addr, mutex);
  handler_(&context);
  return context.ret_val();
}

void PthreadCondSignalWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_cond_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadCondSignalWrapper::CallOriginal(PthreadCondSignalContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_cond_t *), context->cond(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadCondSignalWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                        ADDRINT ret_addr,
                                        pthread_cond_t *cond) {
  PthreadCondSignalContext context(tid, ctxt, ret_addr, cond);
  handler_(&context);
  return context.ret_val();
}

void PthreadCondBroadcastWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_cond_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadCondBroadcastWrapper::CallOriginal(
    PthreadCondBroadcastContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_cond_t *), context->cond(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadCondBroadcastWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                           ADDRINT ret_addr,
                                           pthread_cond_t *cond) {
  PthreadCondBroadcastContext context(tid, ctxt, ret_addr, cond);
  handler_(&context);
  return context.ret_val();
}

void PthreadCondWaitWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_cond_t *),
                                 PIN_PARG(pthread_mutex_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_END);
  }
}

void PthreadCondWaitWrapper::CallOriginal(PthreadCondWaitContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_cond_t *), context->cond(),
      PIN_PARG(pthread_mutex_t *), context->mutex(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadCondWaitWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                      ADDRINT ret_addr, pthread_cond_t *cond,
                                      pthread_mutex_t *mutex) {
  PthreadCondWaitContext context(tid, ctxt, ret_addr, cond, mutex);
  handler_(&context);
  return context.ret_val();
}

void PthreadCondTimedwaitWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_cond_t *),
                                 PIN_PARG(pthread_mutex_t *),
                                 PIN_PARG(struct timespec *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                         IARG_END);
  }
}

void PthreadCondTimedwaitWrapper::CallOriginal(
    PthreadCondTimedwaitContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_cond_t *), context->cond(),
      PIN_PARG(pthread_mutex_t *), context->mutex(),
      PIN_PARG(struct timespec *), context->abstime(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadCondTimedwaitWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                           ADDRINT ret_addr,
                                           pthread_cond_t *cond,
                                           pthread_mutex_t *mutex,
                                           struct timespec *abstime) {
  PthreadCondTimedwaitContext context(tid, ctxt, ret_addr, cond, mutex,
                                      abstime);
  handler_(&context);
  return context.ret_val();
}

void PthreadBarrierInitWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_barrier_t *),
                                 PIN_PARG(pthread_barrierattr_t *),
                                 PIN_PARG(unsigned int),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                         IARG_END);
  }
}

void PthreadBarrierInitWrapper::CallOriginal(
    PthreadBarrierInitContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_barrier_t *), context->barrier(),
      PIN_PARG(pthread_barrierattr_t *), context->attr(),
      PIN_PARG(unsigned int), context->count(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadBarrierInitWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                         ADDRINT ret_addr,
                                         pthread_barrier_t *barrier,
                                         pthread_barrierattr_t *attr,
                                         unsigned int count) {
  PthreadBarrierInitContext context(tid, ctxt, ret_addr, barrier, attr, count);
  handler_(&context);
  return context.ret_val();
}

void PthreadBarrierWaitWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pthread_barrier_t *),
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void PthreadBarrierWaitWrapper::CallOriginal(
    PthreadBarrierWaitContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pthread_barrier_t *), context->barrier(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int PthreadBarrierWaitWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                         ADDRINT ret_addr,
                                         pthread_barrier_t *barrier) {
  PthreadBarrierWaitContext context(tid, ctxt, ret_addr, barrier);
  handler_(&context);
  return context.ret_val();
}

void register_pthread_wrappers(IMG img) {
  PthreadCreateWrapper::Register(img, "pthread_create", "libpthread");
  PthreadJoinWrapper::Register(img, "pthread_join", "libpthread");
  PthreadMutexTryLockWrapper::Register(img, "pthread_mutex_trylock", "libpthread");
  PthreadMutexLockWrapper::Register(img, "pthread_mutex_lock", "libpthread");
  PthreadMutexUnlockWrapper::Register(img, "pthread_mutex_unlock", "libpthread");
  PthreadCondSignalWrapper::Register(img, "pthread_cond_signal", "libpthread");
  PthreadCondBroadcastWrapper::Register(img, "pthread_cond_broadcast", "libpthread");
  PthreadCondWaitWrapper::Register(img, "pthread_cond_wait", "libpthread");
  PthreadCondTimedwaitWrapper::Register(img, "pthread_cond_timedwait", "libpthread");
  PthreadBarrierInitWrapper::Register(img, "pthread_barrier_init", "libpthread");
  PthreadBarrierWaitWrapper::Register(img, "pthread_barrier_wait", "libpthread");
}

