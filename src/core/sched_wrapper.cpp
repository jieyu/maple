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

// File: core/sched_wrapper.hpp - Implementation of sched function wrappers.

#include "core/sched_wrapper.hpp"

void SchedSetSchedulerWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pid_t),
                                 PIN_PARG(int),
                                 PIN_PARG(struct sched_param *),
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

void SchedSetSchedulerWrapper::CallOriginal(SchedSetSchedulerContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pid_t), context->pid(),
      PIN_PARG(int), context->policy(),
      PIN_PARG(struct sched_param *), context->param(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int SchedSetSchedulerWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                       ADDRINT ret_addr, pid_t pid,
                                       int policy, struct sched_param *param) {
  SchedSetSchedulerContext context(tid, ctxt, ret_addr, pid, policy, param);
  handler_(&context);
  return context.ret_val();
}

void SchedYieldWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG_END());

    // replace the original function with the wrapper
    RTN_ReplaceSignature(rtn, (AFUNPTR)__Wrapper,
                         IARG_PROTOTYPE, proto,
                         IARG_THREAD_ID,
                         IARG_CONST_CONTEXT,
                         IARG_RETURN_IP,
                         IARG_END);
  }
}

void SchedYieldWrapper::CallOriginal(SchedYieldContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int SchedYieldWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                 ADDRINT ret_addr) {
  SchedYieldContext context(tid, ctxt, ret_addr);
  handler_(&context);
  return context.ret_val();
}

void SchedSetAffinityWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(pid_t),
                                 PIN_PARG(size_t),
                                 PIN_PARG(cpu_set_t *),
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

void SchedSetAffinityWrapper::CallOriginal(SchedSetAffinityContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(pid_t), context->pid(),
      PIN_PARG(size_t), context->cpusetsize(),
      PIN_PARG(cpu_set_t *), context->cpuset(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int SchedSetAffinityWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                       ADDRINT ret_addr, pid_t pid,
                                       size_t cpusetsize, cpu_set_t *cpuset) {
  SchedSetAffinityContext context(tid, ctxt, ret_addr, pid, cpusetsize, cpuset);
  handler_(&context);
  return context.ret_val();
}

void SetPriorityWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(int),
                                 PIN_PARG(int),
                                 PIN_PARG(int),
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

void SetPriorityWrapper::CallOriginal(SetPriorityContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(int), context->which(),
      PIN_PARG(int), context->who(),
      PIN_PARG(int), context->prio(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int SetPriorityWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                                  int which, int who, int prio) {
  SetPriorityContext context(tid, ctxt, ret_addr, which, who, prio);
  handler_(&context);
  return context.ret_val();
}

void register_sched_wrappers(IMG img) {
  SchedSetSchedulerWrapper::Register(img, "sched_setscheduler", "libc");
  SchedYieldWrapper::Register(img, "sched_yield", "libc");
  SchedSetAffinityWrapper::Register(img, "sched_setaffinity", "libc");
  SetPriorityWrapper::Register(img, "setpriority", "libc");
}

