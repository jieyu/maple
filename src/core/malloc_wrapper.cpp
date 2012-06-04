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

// File: core/malloc_wrapper.cpp - Implementation of memory allocation
// function wrappers.

#include "core/malloc_wrapper.hpp"

#include "core/logging.h"

void MallocWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(size_t),
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

void MallocWrapper::CallOriginal(MallocContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  void *ret_val = NULL;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void *), &ret_val,
      PIN_PARG(size_t), context->size(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

void *MallocWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                               size_t size) {
  MallocContext context(tid, ctxt, ret_addr, size);
  handler_(&context);
  return context.ret_val();
}

void CallocWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(size_t),
                                 PIN_PARG(size_t),
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

void CallocWrapper::CallOriginal(CallocContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  void *ret_val = NULL;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void *), &ret_val,
      PIN_PARG(size_t), context->nmemb(),
      PIN_PARG(size_t), context->size(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

void *CallocWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                               size_t nmemb, size_t size) {
  CallocContext context(tid, ctxt, ret_addr, nmemb, size);
  handler_(&context);
  return context.ret_val();
}

void ReallocWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(void *),
                                 PIN_PARG(size_t),
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

void ReallocWrapper::CallOriginal(ReallocContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  void *ret_val = NULL;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void *), &ret_val,
      PIN_PARG(void *), context->ptr(),
      PIN_PARG(size_t), context->size(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

void *ReallocWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                                void *ptr, size_t size) {
  ReallocContext context(tid, ctxt, ret_addr, ptr, size);
  handler_(&context);
  return context.ret_val();
}

void FreeWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, func_name_,
                                 PIN_PARG(void *),
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

void FreeWrapper::CallOriginal(FreeContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void),
      PIN_PARG(void *), context->ptr(),
      PIN_PARG_END());
}

void FreeWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                            void *ptr) {
  FreeContext context(tid, ctxt, ret_addr, ptr);
  handler_(&context);
}

void VallocWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(size_t),
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

void VallocWrapper::CallOriginal(VallocContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  void *ret_val = NULL;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void *), &ret_val,
      PIN_PARG(size_t), context->size(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

void *VallocWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                               size_t size) {
  VallocContext context(tid, ctxt, ret_addr, size);
  handler_(&context);
  return context.ret_val();
}

void MemalignWrapper::Replace(IMG img, HandlerType handler) {
  RTN rtn = FindRTN(img, func_name_);
  if (RTN_Valid(rtn)) {
    handler_ = handler;

    // set function prototype
    PROTO proto = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                 func_name_,
                                 PIN_PARG(size_t),
                                 PIN_PARG(size_t),
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

void MemalignWrapper::CallOriginal(MemalignContext *context) {
  DEBUG_ASSERT(ori_funptr_);

  void *ret_val = NULL;

  // call original function
  PIN_CallApplicationFunction(context->ctxt(), context->tid(),
      CALLINGSTD_DEFAULT, ori_funptr_,
      PIN_PARG(void *), &ret_val,
      PIN_PARG(size_t), context->boundary(),
      PIN_PARG(size_t), context->size(),
      PIN_PARG_END());

  // set return value
  context->set_ret_val(ret_val);
}

void *MemalignWrapper::__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                                 size_t boundary, size_t size) {
  MemalignContext context(tid, ctxt, ret_addr, boundary, size);
  handler_(&context);
  return context.ret_val();
}

void register_malloc_wrappers(IMG img) {
  MallocWrapper::Register(img, "malloc", "libc");
  CallocWrapper::Register(img, "calloc", "libc");
  ReallocWrapper::Register(img, "realloc", "libc");
  FreeWrapper::Register(img, "free", "libc");
  VallocWrapper::Register(img, "valloc", "libc");
}

