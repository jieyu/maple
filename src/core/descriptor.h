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

// File: core/descriptor.h - Define the general descriptor for
// instrumenting the program.

#ifndef CORE_DESCRIPTOR_H_
#define CORE_DESCRIPTOR_H_

#include "core/basictypes.h"

// the general descriptor for instrumenting the program.
class Descriptor {
 public:
  Descriptor();
  ~Descriptor() {}

  void Merge(Descriptor *desc);
  bool HookMem() { return hook_before_mem_ || hook_after_mem_; }
  bool HookBeforeMem() { return hook_before_mem_; }
  bool HookAfterMem() { return hook_after_mem_; }
  bool HookAtomicInst() { return hook_atomic_inst_; }
  bool HookPthreadFunc() { return hook_pthread_func_; }
  bool HookYieldFunc() { return hook_yield_func_; }
  bool HookMallocFunc() { return hook_malloc_func_; }
  bool HookMainFunc() { return hook_main_func_; }
  bool HookCallReturn() { return hook_call_return_; }
  bool HookSyscall() { return hook_syscall_; }
  bool HookSignal() { return hook_signal_; }
  bool TrackInstCount() { return track_inst_count_; }
  bool TrackCallStack() { return track_call_stack_; }
  bool SkipStackAccess() { return skip_stack_access_; }

  void SetHookBeforeMem() { hook_before_mem_ = true; }
  void SetHookAfterMem() { hook_after_mem_ = true; }
  void SetHookPthreadFunc() { hook_pthread_func_ = true; }
  void SetHookYieldFunc() { hook_yield_func_ = true; }
  void SetHookMallocFunc() { hook_malloc_func_ = true; }
  void SetHookMainFunc() { hook_main_func_ = true; }
  void SetHookCallReturn() { hook_call_return_ = true; }
  void SetHookSyscall() { hook_syscall_ = true; }
  void SetHookSignal() { hook_signal_ = true; }
  void SetHookAtomicInst() { hook_atomic_inst_ = true; }
  void SetTrackInstCount() { track_inst_count_ = true; }
  void SetTrackCallStack() { track_call_stack_ = true; }
  void SetNoSkipStackAccess() { skip_stack_access_ = false; }

 protected:
  bool hook_before_mem_;
  bool hook_after_mem_;
  bool hook_atomic_inst_;
  bool hook_pthread_func_;
  bool hook_yield_func_;
  bool hook_malloc_func_;
  bool hook_main_func_;
  bool hook_call_return_;
  bool hook_syscall_;
  bool hook_signal_;
  bool track_inst_count_;
  bool track_call_stack_;
  bool skip_stack_access_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Descriptor);
};

#endif

