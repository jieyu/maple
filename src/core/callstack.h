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

// File: core/callstack.h - Tracking runtime call stacks.

#ifndef CORE_CALLSTACK_H_
#define CORE_CALLSTACK_H_

#include <vector>
#include <map>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/analyzer.h"

// This class represents a runtime call stack of a thread.
class CallStack {
 public:
  // Define the type for call stack signatures.
  typedef uint64 signature_t;

  CallStack() : signature_(0) {}
  ~CallStack() {}

  signature_t signature() { return signature_; }
  void OnCall(Inst *inst, address_t ret);
  void OnReturn(Inst *inst, address_t target);
  std::string ToString();

 protected:
  typedef std::vector<Inst *> InstVec;
  typedef std::vector<address_t> TargetVec;

  signature_t signature_; // The current call stack signature.
  InstVec inst_vec_;
  TargetVec target_vec_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(CallStack);
};

// This class stores information about runtime call stacks (all threads). This
// class will be used by all analyzers to get call stack information.
class CallStackInfo {
 public:
  explicit CallStackInfo(Mutex *lock) : internal_lock_(lock) {}
  ~CallStackInfo() {}

  // Return the call stack by its thread id.
  CallStack *GetCallStack(thread_id_t thd_id);

 protected:
  typedef std::map<thread_id_t, CallStack *> StackMap;

  Mutex *internal_lock_;
  StackMap stack_map_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(CallStackInfo);
};

// The call stack tracker analyzer is used to track runtime call
// stacks by monitoring every call and return.
class CallStackTracker : public Analyzer {
 public:
  explicit CallStackTracker(CallStackInfo *callstack_info);
  ~CallStackTracker() {}

  void Register() {}
  void AfterCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                 Inst *inst, address_t target, address_t ret);
  void AfterReturn(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, address_t target);

 private:
  DISALLOW_COPY_CONSTRUCTORS(CallStackTracker);
};

#endif

