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

// File: core/sched_wrapper.hpp - Define sched function wrappers.
//
// Here are the prototypes of all the sched functions on my machine
// (x86_64 RHEL5 with glibc-2.5): (* means implemented)
//
//   int sched_setparam (__pid_t __pid, __const struct sched_param *__param) __THROW;
//   int sched_getparam (__pid_t __pid, struct sched_param *__param) __THROW;
// * int sched_setscheduler (__pid_t __pid, int __policy, __const struct sched_param *__param) __THROW;
//   int sched_getscheduler (__pid_t __pid) __THROW;
//   int sched_yield (void) __THROW;
//   int sched_get_priority_max (int __algorithm) __THROW;
//   int sched_get_priority_min (int __algorithm) __THROW;
//   int sched_rr_get_interval (__pid_t __pid, struct timespec *__t) __THROW;
// * int sched_setaffinity (__pid_t __pid, size_t __cpusetsize, __const cpu_set_t *__cpuset) __THROW;
//   int sched_getaffinity (__pid_t __pid, size_t __cpusetsize, cpu_set_t *__cpuset) __THROW;
//   int getpriority (__priority_which_t __which, id_t __who) __THROW;
//   int setpriority (__priority_which_t __which, id_t __who, int __prio) __THROW;

#ifndef CORE_SCHED_WRAPPER_HPP_
#define CORE_SCHED_WRAPPER_HPP_

#include <sched.h>

#include "pin.H"

#include "core/basictypes.h"
#include "core/wrapper.hpp"

// Wrapper context for "sched_setscheduler". The original function
// prototype is:
//
// int sched_setscheduler (pid_t pid, int policy,
//                         const struct sched_param *param);
//
class SchedSetSchedulerContext : public WrapperContext {
 public:
  SchedSetSchedulerContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                           pid_t pid, int policy, struct sched_param *param)
      : WrapperContext(tid, ctxt, ret_addr),
        pid_(pid),
        policy_(policy),
        param_(param) {}
  ~SchedSetSchedulerContext() {}

  int ret_val() { return ret_val_; }
  pid_t pid() { return pid_; }
  int policy() { return policy_; }
  struct sched_param *param() { return param_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pid_t pid_;
  int policy_;
  struct sched_param *param_;

  DISALLOW_COPY_CONSTRUCTORS(SchedSetSchedulerContext);
};

// Wrapper for "sched_setscheduler".
class SchedSetSchedulerWrapper
    : public Wrapper<SchedSetSchedulerContext, SchedSetSchedulerWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(SchedSetSchedulerContext *context);

 private:
  SchedSetSchedulerWrapper() {}
  ~SchedSetSchedulerWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pid_t pid, int policy, struct sched_param *param);

  DISALLOW_COPY_CONSTRUCTORS(SchedSetSchedulerWrapper);
};

// Wrapper context for "sched_yield". The original function
// prototype is:
//
// int sched_yield (void) __THROW;
//
class SchedYieldContext : public WrapperContext {
 public:
  SchedYieldContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr)
      : WrapperContext(tid, ctxt, ret_addr) {}
  ~SchedYieldContext() {}

  int ret_val() { return ret_val_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;

  DISALLOW_COPY_CONSTRUCTORS(SchedYieldContext);
};

// Wrapper for "sched_yield".
class SchedYieldWrapper
    : public Wrapper<SchedYieldContext, SchedYieldWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(SchedYieldContext *context);

 private:
  SchedYieldWrapper() {}
  ~SchedYieldWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr);

  DISALLOW_COPY_CONSTRUCTORS(SchedYieldWrapper);
};

// Wrapper context for "sched_setaffinity". The original function
// prototype is:
//
// int sched_setaffinity (pid_t pid, size_t cpusetsize,
//                        const cpu_set_t *cpuset);
//
class SchedSetAffinityContext : public WrapperContext {
 public:
  SchedSetAffinityContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                          pid_t pid, size_t cpusetsize, cpu_set_t *cpuset)
      : WrapperContext(tid, ctxt, ret_addr),
        pid_(pid),
        cpusetsize_(cpusetsize),
        cpuset_(cpuset) {}
  ~SchedSetAffinityContext() {}

  int ret_val() { return ret_val_; }
  pid_t pid() { return pid_; }
  size_t cpusetsize() { return cpusetsize_; }
  cpu_set_t *cpuset() { return cpuset_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  pid_t pid_;
  size_t cpusetsize_;
  cpu_set_t *cpuset_;

  DISALLOW_COPY_CONSTRUCTORS(SchedSetAffinityContext);
};

// Wrapper for "sched_setaffinity".
class SchedSetAffinityWrapper
    : public Wrapper<SchedSetAffinityContext, SchedSetAffinityWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(SchedSetAffinityContext *context);

 private:
  SchedSetAffinityWrapper() {}
  ~SchedSetAffinityWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       pid_t pid, size_t cpusetsize, cpu_set_t *cpuset);

  DISALLOW_COPY_CONSTRUCTORS(SchedSetAffinityWrapper);
};

// Wrapper context for "setpriority". The original function prototype is:
//
// int setpriority (int which, int who, int prio);
//
class SetPriorityContext : public WrapperContext {
 public:
  SetPriorityContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                     int which, int who, int prio)
      : WrapperContext(tid, ctxt, ret_addr),
        which_(which),
        who_(who),
        prio_(prio) {}
  ~SetPriorityContext() {}

  int ret_val() { return ret_val_; }
  int which() { return which_; }
  int who() { return who_; }
  int prio() { return prio_; }
  void set_ret_val(int ret_val) { ret_val_ = ret_val; }

 private:
  int ret_val_;
  int which_;
  int who_;
  int prio_;

  DISALLOW_COPY_CONSTRUCTORS(SetPriorityContext);
};

// Wrapper for "setpriority".
class SetPriorityWrapper
    : public Wrapper<SetPriorityContext, SetPriorityWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(SetPriorityContext *context);

 private:
  SetPriorityWrapper() {}
  ~SetPriorityWrapper() {}

  static int __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                       int which, int who, int prio);

  DISALLOW_COPY_CONSTRUCTORS(SetPriorityWrapper);
};

// Global functions to install sched wrappers
void register_sched_wrappers(IMG img);

#endif

