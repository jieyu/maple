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

// File: core/callstack.cc - Tracking runtime call stacks.

#include "core/callstack.h"

#include "core/logging.h"

void CallStack::OnCall(Inst *inst, address_t ret) {
  inst_vec_.push_back(inst);
  target_vec_.push_back(ret);

  // We want the signature to be unique across runs. Therefore, we should not
  // use the pointer value. Instead, we should use the inst id.
  signature_ += (signature_t)inst->id();

  DEBUG_FMT_PRINT_SAFE("(%s)\n", ToString().c_str());
}

void CallStack::OnReturn(Inst *inst, address_t target) {
  DEBUG_ASSERT(inst_vec_.size() == target_vec_.size());

  // Handle the empty stack case.
  assert(!inst_vec_.empty());

  // Backward search matching target address, find the first one that matches
  // and remove the entires after it. If the target address is not found in the
  // stack, ignore this return. (This is caused by PIN's non-transparent wrapper
  // implementation. The return address could be the address in the stub created
  // by PIN.)
  bool found = false;
  int size = (int)inst_vec_.size();
  int new_size = size; // The new stack size after return;
  signature_t new_signature = signature_; // The new signature after return;

  for (int idx = size - 1; idx >= 0; idx--) {
    Inst *inst = inst_vec_[idx];
    address_t addr = target_vec_[idx];

    new_size -= 1;
    new_signature -= inst->id();

    if (addr == target) {
      found = true;
      break;
    }
  }

  if (found) {
    inst_vec_.erase(inst_vec_.begin() + new_size, inst_vec_.end());
    target_vec_.erase(target_vec_.begin() + new_size, target_vec_.end());
    signature_ = new_signature;
  }

  DEBUG_FMT_PRINT_SAFE("(%s)\n", ToString().c_str());
}

std::string CallStack::ToString() {
  std::stringstream ss;
  ss << std::hex;
  size_t size = inst_vec_.size();
  for (size_t i = 0; i < size; i++) {
    ss << "<" << inst_vec_[i]->id() << " ";
    ss << "0x" << target_vec_[i] << ">";
    if (i != size - 1)
      ss << " ";
  }
  return ss.str();
}

CallStack *CallStackInfo::GetCallStack(thread_id_t thd_id) {
  ScopedLock locker(internal_lock_);

  StackMap::iterator it = stack_map_.find(thd_id);
  if (it == stack_map_.end()) {
    CallStack *callstack = new CallStack;
    stack_map_[thd_id] = callstack;
    return callstack;
  } else {
    return it->second;
  }
}

CallStackTracker::CallStackTracker(CallStackInfo *callstack_info) {
  DEBUG_ASSERT(callstack_info);
  set_callstack_info(callstack_info);

  // Need to monitor calls and returns.
  desc_.SetHookCallReturn();
}

void CallStackTracker::AfterCall(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk,
                                 Inst *inst,
                                 address_t target,
                                 address_t ret) {
  CallStack *callstack = callstack_info_->GetCallStack(curr_thd_id);
  callstack->OnCall(inst, ret);
}

void CallStackTracker::AfterReturn(thread_id_t curr_thd_id,
                                   timestamp_t curr_thd_clk,
                                   Inst *inst,
                                   address_t target) {
  CallStack *callstack = callstack_info_->GetCallStack(curr_thd_id);
  callstack->OnReturn(inst, target);
}

