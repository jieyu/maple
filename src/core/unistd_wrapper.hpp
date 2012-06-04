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

// File: core/unistd_wrapper.hpp - Define unistd function wrappers.
//
// Here are the prototypes of some the unistd functions on my machine
// (x86_64 RHEL5 with glibc-2.5): (* means implemented)
//
// * unsigned int sleep (unsigned int __seconds);
//   int usleep (__useconds_t __useconds);

#ifndef CORE_UNISTD_WRAPPER_HPP_
#define CORE_UNISTD_WRAPPER_HPP_

#include "pin.H"

#include "core/basictypes.h"
#include "core/wrapper.hpp"

// Wrapper context for "sleep". The original function prototype is:
//
// unsigned int sleep (unsigned int seconds);
//
class SleepContext : public WrapperContext {
 public:
  SleepContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
               unsigned int seconds)
      : WrapperContext(tid, ctxt, ret_addr),
        seconds_(seconds) {}
  ~SleepContext() {}

  unsigned int ret_val() { return ret_val_; }
  unsigned int seconds() { return seconds_; }
  void set_ret_val(unsigned int ret_val) { ret_val_ = ret_val; }

 private:
  unsigned int ret_val_;
  unsigned int seconds_;

  DISALLOW_COPY_CONSTRUCTORS(SleepContext);
};

// Wrapper for "sleep".
class SleepWrapper
    : public Wrapper<SleepContext, SleepWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(SleepContext *context);

 private:
  SleepWrapper() {}
  ~SleepWrapper() {}

  static unsigned int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                                unsigned int seconds);

  DISALLOW_COPY_CONSTRUCTORS(SleepWrapper);
};

// Wrapper context for "usleep". The original function prototype is:
//
// int usleep (useconds_t useconds);
//
class UsleepContext : public WrapperContext {
 public:
  UsleepContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                useconds_t useconds)
      : WrapperContext(tid, ctxt, ret_addr),
        useconds_(useconds) {}
  ~UsleepContext() {}

  int ret_val() { return ret_val_; }
  useconds_t useconds() { return useconds_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  useconds_t useconds_;

  DISALLOW_COPY_CONSTRUCTORS(UsleepContext);
};

// Wrapper for "usleep".
class UsleepWrapper
    : public Wrapper<UsleepContext, UsleepWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(UsleepContext *context);

 private:
  UsleepWrapper() {}
  ~UsleepWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       useconds_t useconds);

  DISALLOW_COPY_CONSTRUCTORS(UsleepWrapper);
};

// Global functions to install unistd wrappers
void register_unistd_wrappers(IMG img);

#endif

