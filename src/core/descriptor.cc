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

// File: core/descriptor.cc - Implementation of the general descriptor
// for instrumenting the program.

#include "core/descriptor.h"

Descriptor::Descriptor()
    : hook_before_mem_(false),
      hook_after_mem_(false),
      hook_atomic_inst_(false),
      hook_pthread_func_(false),
      hook_yield_func_(false),
      hook_malloc_func_(false),
      hook_main_func_(false),
      hook_call_return_(false),
      hook_syscall_(false),
      hook_signal_(false),
      track_inst_count_(false),
      track_call_stack_(false),
      skip_stack_access_(true) {
  // empty
}

void Descriptor::Merge(Descriptor *desc) {
  hook_before_mem_ = hook_before_mem_ || desc->hook_before_mem_;
  hook_after_mem_ = hook_after_mem_ || desc->hook_after_mem_;
  hook_atomic_inst_ = hook_atomic_inst_ || desc->hook_atomic_inst_;
  hook_pthread_func_ = hook_pthread_func_ || desc->hook_pthread_func_;
  hook_yield_func_ = hook_yield_func_ || desc->hook_yield_func_;
  hook_malloc_func_ = hook_malloc_func_ || desc->hook_malloc_func_;
  hook_main_func_ = hook_main_func_ || desc->hook_main_func_;
  hook_call_return_ = hook_call_return_ || desc->hook_call_return_;
  hook_syscall_ = hook_syscall_ || desc->hook_syscall_;
  hook_signal_ = hook_signal_ || desc->hook_signal_;
  track_inst_count_ = track_inst_count_ || desc->track_inst_count_;
  track_call_stack_ = track_call_stack_ || desc->track_call_stack_;
  skip_stack_access_ = skip_stack_access_ && desc->skip_stack_access_;
}

