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

// File: core/unistd_wrapper.cpp - Implementation of unistd function
// wrappers.

#include "core/unistd_wrapper.hpp"

void SleepWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(unsigned int), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(unsigned int),
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

void SleepWrapper::CallOriginal(SleepContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  unsigned int ret_val = 0;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(unsigned int), &ret_val,
      PIN_PARG(unsigned int), context->seconds(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

unsigned int SleepWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                                     ADDRINT ret_addr, unsigned int seconds) {
  SleepContext context(tid, ctxt, ret_addr, seconds);
  handler_(&context);
  return context.ret_val();
}

void UsleepWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(useconds_t),
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

void UsleepWrapper::CallOriginal(UsleepContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  int ret_val = -1;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(int), &ret_val,
      PIN_PARG(useconds_t), context->useconds(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

int UsleepWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt,
                             ADDRINT ret_addr, useconds_t useconds) {
  UsleepContext context(tid, ctxt, ret_addr, useconds);
  handler_(&context);
  return context.ret_val();
}

void register_unistd_wrappers(IMG img) {
  SleepWrapper::Register(img, "sleep", "libc");
  UsleepWrapper::Register(img, "usleep", "libc");
}

