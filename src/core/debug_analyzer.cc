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

// File: core/debug_analyzer.cc - The analyzer for debug purpose.

#include "core/debug_analyzer.h"

void DebugAnalyzer::Register() {
  knob_->RegisterBool("enable_debug", "whether enable the debug analyzer", "0");
  knob_->RegisterBool("debug_mem", "whether debug mem accesses", "0");
  knob_->RegisterBool("debug_atomic", "whether debug atomic inst", "0");
  knob_->RegisterBool("debug_main", "whether debug main functions", "0");
  knob_->RegisterBool("debug_call_return", "whether debug calls and returns", "0");
  knob_->RegisterBool("debug_pthread", "whether debug pthread functions", "0");
  knob_->RegisterBool("debug_malloc", "whether debug malloc functions", "0");
  knob_->RegisterBool("debug_syscall", "whether debug system calls", "0");
  knob_->RegisterBool("debug_track_clk", "whether track per thread clock", "1");
  knob_->RegisterBool("debug_track_callstack", "whether track runtime call stack", "0");
}

bool DebugAnalyzer::Enabled() {
  return knob_->ValueBool("enable_debug");
}

void DebugAnalyzer::Setup() {
  if (knob_->ValueBool("debug_mem"))
    desc_.SetHookBeforeMem();
  if (knob_->ValueBool("debug_atomic"))
    desc_.SetHookAtomicInst();
  if (knob_->ValueBool("debug_main"))
    desc_.SetHookMainFunc();
  if (knob_->ValueBool("debug_call_return"))
    desc_.SetHookCallReturn();
  if (knob_->ValueBool("debug_pthread"))
    desc_.SetHookPthreadFunc();
  if (knob_->ValueBool("debug_malloc"))
    desc_.SetHookMallocFunc();
  if (knob_->ValueBool("debug_syscall"))
    desc_.SetHookSyscall();
  if (knob_->ValueBool("debug_track_clk"))
    desc_.SetTrackInstCount();
  if (knob_->ValueBool("debug_track_callstack"))
    desc_.SetTrackCallStack();
}

