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

// File: systematic/program.cc - The abstract modeling of a multithreaded
// program and its execution.

#include "systematic/program.h"

#include <sstream>
#include "core/logging.h"

namespace systematic {

Thread::hash_val_t Thread::Hash() {
  hash_val_t hash_val = 0;
  if (creator_)
    hash_val += (hash_val_t)creator_->uid_;
  hash_val += (hash_val_t)creator_idx_;
  return hash_val;
}

bool Thread::Match(Thread *thd) {
  if (!creator_) {
    if (!thd->creator_)
      return true;
    else
      return false;
  } else {
    if (!thd->creator_)
      return false;
    else
      return creator_->uid_ == thd->creator_->uid_ &&
             creator_idx_ == thd->creator_idx_;
  }
}

Object::hash_val_t SObject::Hash() {
  hash_val_t hash_val = 0;
  hash_val += (hash_val_t)image_->id();
  hash_val += (hash_val_t)offset_;
  return hash_val;
}

bool SObject::Match(Object *obj) {
  SObject *sobj = dynamic_cast<SObject *>(obj);
  if (!sobj)
    return false;
  return image_->id() == sobj->image_->id() &&
         offset_ == sobj->offset_;
}

Object::hash_val_t DObject::Hash() {
  hash_val_t hash_val = 0;
  hash_val += (hash_val_t)creator_->uid();
  hash_val += (hash_val_t)creator_inst_->id();
  hash_val += (hash_val_t)creator_idx_;
  hash_val += (hash_val_t)offset_;
  return hash_val;
}

bool DObject::Match(Object *obj) {
  DObject *dobj = dynamic_cast<DObject *>(obj);
  if (!dobj)
    return false;
  return creator_->uid() == dobj->creator_->uid() &&
         creator_inst_->id() == dobj->creator_inst_->id() &&
         creator_idx_ == dobj->creator_idx_ &&
         offset_ == dobj->offset_;
}

Thread *Program::GetMainThread() {
  // main thread always has uid == 1
  Thread *main_thd = FindThread(1);
  if (!main_thd) {
    // create the main thread
    DEBUG_ASSERT(!curr_thd_uid_);
    main_thd = new Thread;
    main_thd->uid_ = ++curr_thd_uid_;
    thd_uid_table_[main_thd->uid_] = main_thd;
    thd_hash_table_[main_thd->Hash()].push_back(main_thd);
  }
  return main_thd;
}

Thread *Program::GetThread(Thread *creator, Thread::idx_t creator_idx) {
  DEBUG_ASSERT(creator);
  // check hash table
  Thread thd;
  thd.creator_ = creator;
  thd.creator_idx_ = creator_idx;
  Thread::hash_val_t hash_val = thd.Hash();
  Thread::Vec &vec = thd_hash_table_[hash_val];
  for (Thread::Vec::iterator vit = vec.begin(); vit != vec.end(); ++vit) {
    Thread *ithd = *vit;
    if (thd.Match(ithd))
      return ithd;
  }
  // create a new thread
  Thread *new_thd = new Thread;
  new_thd->uid_ = ++curr_thd_uid_;
  new_thd->creator_ = creator;
  new_thd->creator_idx_ = creator_idx;
  thd_uid_table_[new_thd->uid_] = new_thd;
  thd_hash_table_[hash_val].push_back(new_thd);
  return new_thd;
}

SObject *Program::GetSObject(Image *image, address_t offset) {
  // check the hash table
  SObject sobj;
  sobj.image_ = image;
  sobj.offset_ = offset;
  Object::hash_val_t hash_val = sobj.Hash();
  Object::Vec &vec = obj_hash_table_[hash_val];
  for (Object::Vec::iterator vit = vec.begin(); vit != vec.end(); ++vit) {
    Object *iobj = *vit;
    if (sobj.Match(iobj))
      return dynamic_cast<SObject *>(iobj);
  }
  // create a new static object
  SObject *new_sobj = new SObject;
  new_sobj->uid_ = ++curr_obj_uid_;
  new_sobj->image_ = image;
  new_sobj->offset_ = offset;
  obj_uid_table_[new_sobj->uid_] = new_sobj;
  obj_hash_table_[hash_val].push_back(new_sobj);
  return new_sobj;
}

DObject *Program::GetDObject(Thread *creator,
                             Inst *creator_inst,
                             Object::idx_t creator_idx,
                             address_t offset) {
  DEBUG_ASSERT(creator);
  // check the hash table
  DObject dobj;
  dobj.creator_ = creator;
  dobj.creator_inst_ = creator_inst;
  dobj.creator_idx_ = creator_idx;
  dobj.offset_ = offset;
  Object::hash_val_t hash_val = dobj.Hash();
  Object::Vec &vec = obj_hash_table_[hash_val];
  for (Object::Vec::iterator vit = vec.begin(); vit != vec.end(); ++vit) {
    Object *iobj = *vit;
    if (dobj.Match(iobj))
      return dynamic_cast<DObject *>(iobj);
  }
  // create a new dynamic object
  DObject *new_dobj = new DObject;
  new_dobj->uid_ = ++curr_obj_uid_;
  new_dobj->creator_ = creator;
  new_dobj->creator_inst_ = creator_inst;
  new_dobj->creator_idx_ = creator_idx;
  new_dobj->offset_ = offset;
  obj_uid_table_[new_dobj->uid_] = new_dobj;
  obj_hash_table_[hash_val].push_back(new_dobj);
  return new_dobj;
}

Thread *Program::FindThread(Thread::uid_t uid) {
  Thread::UidMap::iterator it = thd_uid_table_.find(uid);
  if (it == thd_uid_table_.end())
    return NULL;
  else
    return it->second;
}

Object *Program::FindObject(Object::uid_t uid) {
  Object::UidMap::iterator it = obj_uid_table_.find(uid);
  if (it == obj_uid_table_.end())
    return NULL;
  else
    return it->second;
}

void Program::Load(const std::string &db_name, StaticInfo *sinfo) {
  ProgramProto program_proto;
  // load from file
  std::fstream in(db_name.c_str(), std::ios::in | std::ios::binary);
  if (in.is_open())
    program_proto.ParseFromIstream(&in);
  in.close();
  // load thread info, we need two passes
  for (int i = 0; i < program_proto.thread_size(); i++) {
    ThreadProto *proto = program_proto.mutable_thread(i);
    Thread *thd = new Thread;
    thd->uid_ = proto->uid();
    thd_uid_table_[thd->uid_] = thd;
    if (curr_thd_uid_ < thd->uid_)
      curr_thd_uid_ = thd->uid_;
  }
  for (int i = 0; i < program_proto.thread_size(); i++) {
    ThreadProto *proto = program_proto.mutable_thread(i);
    Thread *thd = thd_uid_table_[proto->uid()];
    if (proto->has_creator_uid()) {
      thd->creator_ = FindThread(proto->creator_uid());
      DEBUG_ASSERT(thd->creator_);
      thd->creator_idx_ = proto->creator_idx();
    }
    thd_hash_table_[thd->Hash()].push_back(thd);
  }
  // load static object info
  for (int i = 0; i < program_proto.sobject_size(); i++) {
    SObjectProto *proto = program_proto.mutable_sobject(i);
    SObject *sobj = new SObject;
    sobj->uid_ = proto->uid();
    sobj->image_ = sinfo->FindImage(proto->image_id());
    DEBUG_ASSERT(sobj->image_);
    sobj->offset_ = proto->offset();
    obj_uid_table_[sobj->uid_] = sobj;
    obj_hash_table_[sobj->Hash()].push_back(sobj);
    if (curr_obj_uid_ < sobj->uid_)
      curr_obj_uid_ = sobj->uid_;
  }
  // load dynamic object info
  for (int i = 0; i < program_proto.dobject_size(); i++) {
    DObjectProto *proto = program_proto.mutable_dobject(i);
    DObject *dobj = new DObject;
    dobj->uid_ = proto->uid();
    dobj->creator_ = FindThread(proto->creator_uid());
    DEBUG_ASSERT(dobj->creator_);
    dobj->creator_inst_ = sinfo->FindInst(proto->creator_inst_id());
    DEBUG_ASSERT(dobj->creator_inst_);
    dobj->creator_idx_ = proto->creator_idx();
    dobj->offset_ = proto->offset();
    obj_uid_table_[dobj->uid_] = dobj;
    obj_hash_table_[dobj->Hash()].push_back(dobj);
    if (curr_obj_uid_ < dobj->uid_)
      curr_obj_uid_ = dobj->uid_;
  }
}

void Program::Save(const std::string &db_name, StaticInfo *sinfo) {
  ProgramProto program_proto;
  // save thread info
  for (Thread::UidMap::iterator it = thd_uid_table_.begin();
       it != thd_uid_table_.end(); ++it) {
    Thread *thd = it->second;
    ThreadProto *proto = program_proto.add_thread();
    proto->set_uid(thd->uid_);
    if (thd->creator_) {
      proto->set_creator_uid(thd->creator_->uid_);
      proto->set_creator_idx(thd->creator_idx_);
    }
  }
  // save object info
  for (Object::UidMap::iterator it = obj_uid_table_.begin();
       it != obj_uid_table_.end(); ++it) {
    Object *obj = it->second;
    SObject *sobj = dynamic_cast<SObject *>(obj);
    if (sobj) {
      SObjectProto *proto = program_proto.add_sobject();
      proto->set_uid(sobj->uid_);
      proto->set_image_id(sobj->image_->id());
      proto->set_offset(sobj->offset_);
      continue;
    }
    DObject *dobj = dynamic_cast<DObject *>(obj);
    if (dobj) {
      DObjectProto *proto = program_proto.add_dobject();
      proto->set_uid(dobj->uid_);
      proto->set_creator_uid(dobj->creator_->uid_);
      proto->set_creator_inst_id(dobj->creator_inst_->id());
      proto->set_creator_idx(dobj->creator_idx_);
      proto->set_offset(dobj->offset_);
      continue;
    }
  }
  // save to file
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  program_proto.SerializeToOstream(&out);
  out.close();
}

bool Action::IsThreadOp() {
  switch (op_) {
    case OP_THREAD_START:
    case OP_THREAD_END:
    case OP_THREAD_CREATE:
    case OP_THREAD_JOIN:
      return true;
    default:
      return false;
  }
}

bool Action::IsMutexOp() {
  switch (op_) {
    case OP_MUTEX_LOCK:
    case OP_MUTEX_UNLOCK:
    case OP_MUTEX_TRYLOCK:
      return true;
    default:
      return false;
  }
}

bool Action::IsCondOp() {
  switch (op_) {
    case OP_COND_WAIT:
    case OP_COND_SIGNAL:
    case OP_COND_BROADCAST:
    case OP_COND_TIMEDWAIT:
      return true;
    default:
      return false;
  }
}

bool Action::IsBarrierOp() {
  switch (op_) {
    case OP_BARRIER_INIT:
    case OP_BARRIER_WAIT:
      return true;
    default:
      return false;
  }
}

bool Action::IsMemOp() {
  switch (op_) {
    case OP_MEM_READ:
    case OP_MEM_WRITE:
      return true;
    default:
      return false;
  }
}

bool Action::IsYieldOp() {
  return yield_;
}

bool Action::IsWrite() {
  // return true if this action will actually modify the
  // state of the modeled program.
  return op_ != OP_MEM_READ;
}

std::string Action::ToString() {
  std::stringstream ss;
  ss << "thd = " << std::dec << thd_->uid() << ", ";
  if (obj_)
    ss << "obj = " << std::dec << obj_->uid() << ", ";
  else
    ss << "obj = NULL, ";
  ss << "op = " << op_ << ", ";
  if (inst_)
    ss << "inst = (" << inst_->ToString() << "), ";
  else
    ss << "inst = NULL, ";
  ss << "tc = " << std::dec << tc_ << ", ";
  ss << "oc = " << std::dec << oc_ << ", ";
  ss << "yield = " << yield_;
  return ss.str();
}

std::string State::ToString() {
  std::stringstream ss;
  ss << "enabled:" << std::endl;
  for (Action::Map::iterator it = enabled_.begin(); it != enabled_.end(); ++it){
    Action *action = it->second;
    ss << "   " << action->ToString() << std::endl;
  }
  ss << "taken:" << std::endl;
  if (taken_)
    ss << "   " << taken_->ToString();
  else
    ss << "   NULL";
  return ss.str();
}

Action *Execution::CreateAction(Thread *thd,
                                Object *obj,
                                Operation op,
                                Inst *inst) {
  Action *action = new Action;
  action->exec_ = this;
  action->idx_ = action_vec_.size();
  action->thd_ = thd;
  action->obj_ = obj;
  action->op_ = op;
  action->inst_ = inst;
  action_vec_.push_back(action);
  return action;
}

State *Execution::CreateState() {
  State *state = new State;
  state->exec_ = this;
  state->idx_ = state_vec_.size();
  state_vec_.push_back(state);
  return state;
}

State *Execution::Prev(State *state) {
  if (state->idx_ == 0)
    return NULL;
  else
    return state_vec_[state->idx_ - 1];
}

State *Execution::Next(State *state) {
  size_t next_idx = state->idx_ + 1;
  if (next_idx >= state_vec_.size())
    return NULL;
  else
    return state_vec_[next_idx];
}

State *Execution::FindState(size_t idx) {
  if (idx >= state_vec_.size())
    return NULL;
  return state_vec_[idx];
}

void Execution::Load(const std::string &db_name,
                     StaticInfo *sinfo,
                     Program *program) {
  ExecutionProto exec_proto;
  // load from file
  std::fstream in(db_name.c_str(), std::ios::in | std::ios::binary);
  if (in.is_open())
    exec_proto.ParseFromIstream(&in);
  in.close();
  // load actions
  for (int i = 0; i < exec_proto.action_size(); i++) {
    ActionProto *action_proto = exec_proto.mutable_action(i);
    Action *action = new Action;
    action->exec_ = this;
    action->idx_ = action_vec_.size();
    action->thd_ = program->FindThread(action_proto->thd_uid());
    DEBUG_ASSERT(action->thd_);
    if (action_proto->has_obj_uid()) {
      action->obj_ = program->FindObject(action_proto->obj_uid());
      DEBUG_ASSERT(action->obj_);
    }
    action->op_ = action_proto->op();
    if (action_proto->has_inst_id()) {
      action->inst_ = sinfo->FindInst(action_proto->inst_id());
      DEBUG_ASSERT(action->inst_);
    }
    action->tc_ = action_proto->tc();
    action->oc_ = action_proto->oc();
    action->yield_ = action_proto->yield();
    action_vec_.push_back(action);
  }
  // load states
  for (int i = 0; i < exec_proto.state_size(); i++) {
    StateProto *state_proto = exec_proto.mutable_state(i);
    State *state = new State;
    state->exec_ = this;
    state->idx_ = state_vec_.size();
    // load enabled
    for (int j = 0; j < state_proto->enabled_size(); j++) {
      size_t idx = state_proto->enabled(j);
      DEBUG_ASSERT(idx < action_vec_.size());
      Action *action = action_vec_[idx];
      state->enabled_[action->thd_] = action;
    }
    // load taken
    if (state_proto->has_taken()) {
      size_t idx = state_proto->taken();
      DEBUG_ASSERT(idx < action_vec_.size());
      state->taken_ = action_vec_[idx];
    }
    state_vec_.push_back(state);
  }
}

void Execution::Save(const std::string &db_name,
                     StaticInfo *sinfo,
                     Program *program) {
  ExecutionProto exec_proto;
  // save actions
  for (Action::Vec::iterator it = action_vec_.begin();
       it != action_vec_.end(); ++it) {
    Action *action = *it;
    ActionProto *action_proto = exec_proto.add_action();
    action_proto->set_thd_uid(action->thd_->uid());
    if (action->obj_)
      action_proto->set_obj_uid(action->obj_->uid());
    action_proto->set_op(action->op_);
    if (action->inst_)
      action_proto->set_inst_id(action->inst_->id());
    action_proto->set_tc(action->tc_);
    action_proto->set_oc(action->oc_);
    action_proto->set_yield(action->yield_);
  }
  // save states
  for (State::Vec::iterator it = state_vec_.begin();
       it != state_vec_.end(); ++it) {
    State *state = *it;
    StateProto *state_proto = exec_proto.add_state();
    // save enabled
    for (Action::Map::iterator eit = state->enabled_.begin();
         eit != state->enabled_.end(); ++eit) {
      Action *action = eit->second;
      state_proto->add_enabled(action->idx_);
    }
    // save taken
    if (state->taken_)
      state_proto->set_taken(state->taken_->idx_);
  }
  // save to file
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  exec_proto.SerializeToOstream(&out);
  out.close();
}

} // namespace systematic

