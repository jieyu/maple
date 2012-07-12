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

// File: systematic/search.cc - The implementation for systematic
// interleaving search.

#include "systematic/search.h"

#include <sstream>
#include "core/logging.h"

namespace systematic {

std::string ActionInfo::ToString() {
  std::stringstream ss;
  ss << "thd = " << std::dec << thd_->uid() << ", ";
  if (obj_)
    ss << "obj = " << std::dec << obj_->uid() << ", ";
  else
    ss << "obj = NULL, ";
  ss << "op = " << op_ << ", ";
  if (inst_)
    ss << "inst = (" << inst_->ToString() << ")";
  else
    ss << "inst = NULL";
  return ss.str();
}

bool SearchNode::Finished() {
  // return true if all threads in backtrack set are done
  for (Thread::Set::iterator it = backtrack_.begin();
       it != backtrack_.end(); ++it) {
    Thread *thd = *it;
    if (!IsDone(thd))
      return false;
  }
  return true;
}

std::string SearchNode::ToString() {
  std::stringstream ss;
  ss << "search node " << std::dec << idx_ << std::endl;
  // enabled set
  ss << "   enabled:" << std::endl;
  for (ActionInfo::Map::iterator it = enabled_.begin();
       it != enabled_.end(); ++it) {
    ActionInfo *info = it->second;
    ss << "      " << info->ToString() << std::endl;
  }
  // sel
  if (sel_)
    ss << "   sel = " << std::dec << sel_->uid() << std::endl;
  else
    ss << "   sel = NULL" << std::endl;
  // backtrack set
  ss << "   backtrack = ( ";
  for (Thread::Set::iterator it = backtrack_.begin();
       it != backtrack_.end(); ++it) {
    Thread *thd = *it;
    ss << std::dec << thd->uid() << " ";
  }
  ss << ")" << std::endl;
  // done set
  ss << "   done = ( ";
  for (Thread::Set::iterator it = done_.begin(); it != done_.end(); ++it) {
    Thread *thd = *it;
    ss << std::dec << thd->uid() << " ";
  }
  ss << ")";
  return ss.str();
}

SearchNode *SearchInfo::GetNextNode(State *state) {
  // return NULL if diverge
  DEBUG_ASSERT(cursor_ <= stack_.size());
  SearchNode *node = NULL;
  if (cursor_ == stack_.size()) {
    // we need to create a new node
    node = new SearchNode;
    node->info_ = this;
    node->idx_ = cursor_;
    // create action info
    for (Action::Map::iterator it = state->enabled()->begin();
         it != state->enabled()->end(); ++it) {
      node->enabled_[it->first] = new ActionInfo(it->second);
    }
    stack_.push_back(node);
  } else {
    // return an existing node
    node = stack_[cursor_];
    // check divergencve
    if (!CheckDivergence(node, state))
      return NULL;
  }
  // move the cursor forward
  ++cursor_;
  return node;
}

void SearchInfo::UpdateForNext() {
  while (!stack_.empty()) {
    SearchNode *node = stack_.back();
    if (!node->Finished())
      break;
    stack_.pop_back();
  }
  if (stack_.empty())
    done_ = true;
  num_runs_ += 1;
  DEBUG_FMT_PRINT_SAFE("search info: done = %d, stack size = %d, runs = %d\n",
                       done_, (int)stack_.size(), num_runs_);
}

SearchNode *SearchInfo::Prev(SearchNode *node) {
  if (node->idx_ == 0)
    return NULL;
  else
    return stack_[node->idx_ - 1];
}

SearchNode *SearchInfo::Next(SearchNode *node) {
  size_t next_idx_ = node->idx_ + 1;
  if (next_idx_ >= stack_.size())
    return NULL;
  else
    return stack_[next_idx_];
}

void SearchInfo::Load(const std::string &db_name,
                      StaticInfo *sinfo,
                      Program *program) {
  SearchInfoProto info_proto;
  // load from file
  std::fstream in(db_name.c_str(), std::ios::in | std::ios::binary);
  if (in.is_open())
    info_proto.ParseFromIstream(&in);
  in.close();
  // load general info
  done_ = info_proto.done();
  num_runs_ = info_proto.num_runs();
  // load search stack
  for (int i = 0; i < info_proto.node_size(); i++) {
    SearchNodeProto *node_proto = info_proto.mutable_node(i);
    SearchNode *node = new SearchNode;
    node->info_ = this;
    node->idx_ = stack_.size();
    // load sel
    node->sel_ = program->FindThread(node_proto->sel());
    DEBUG_ASSERT(node->sel_);
    // load backtrack set
    for (int j = 0; j < node_proto->backtrack_size(); j++) {
      Thread *thd = program->FindThread(node_proto->backtrack(j));
      DEBUG_ASSERT(thd);
      node->backtrack_.insert(thd);
    }
    // load done set
    for (int j = 0; j < node_proto->done_size(); j++) {
      Thread *thd = program->FindThread(node_proto->done(j));
      DEBUG_ASSERT(thd);
      node->done_.insert(thd);
    }
    // load enabled set
    for (int j = 0; j < node_proto->enabled_size(); j++) {
      ActionInfoProto *action_info_proto = node_proto->mutable_enabled(j);
      ActionInfo *action_info = new ActionInfo;
      action_info->thd_ = program->FindThread(action_info_proto->thd_uid());
      if (action_info_proto->has_obj_uid())
        action_info->obj_ = program->FindObject(action_info_proto->obj_uid());
      action_info->op_ = action_info_proto->op();
      if (action_info_proto->has_inst_id())
        action_info->inst_ = sinfo->FindInst(action_info_proto->inst_id());
      node->enabled_[action_info->thd_] = action_info;
    }
    stack_.push_back(node);
  }
}

void SearchInfo::Save(const std::string &db_name,
                      StaticInfo *sinfo,
                      Program *program) {
  SearchInfoProto info_proto;
  // save general info
  info_proto.set_done(done_);
  info_proto.set_num_runs(num_runs_);
  // save search stack
  for (SearchNode::Vec::iterator it = stack_.begin(); it != stack_.end(); ++it){
    SearchNode *node = *it;
    SearchNodeProto *node_proto = info_proto.add_node();
    // save sel
    node_proto->set_sel(node->sel_->uid());
    // save backtrack
    for (Thread::Set::iterator bit = node->backtrack_.begin();
         bit != node->backtrack_.end(); ++bit) {
      Thread *thd = *bit;
      node_proto->add_backtrack(thd->uid());
    }
    // save done
    for (Thread::Set::iterator dit = node->done_.begin();
         dit != node->done_.end(); ++dit) {
      Thread *thd = *dit;
      node_proto->add_done(thd->uid());
    }
    // save enabled
    for (ActionInfo::Map::iterator eit = node->enabled_.begin();
         eit != node->enabled_.end(); ++eit) {
      ActionInfo *action_info = eit->second;
      ActionInfoProto *action_info_proto = node_proto->add_enabled();
      action_info_proto->set_thd_uid(action_info->thd_->uid());
      if (action_info->obj_)
        action_info_proto->set_obj_uid(action_info->obj_->uid());
      action_info_proto->set_op(action_info->op_);
      if (action_info->inst_)
        action_info_proto->set_inst_id(action_info->inst_->id());
    }
  }
  // save to file
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  info_proto.SerializeToOstream(&out);
  out.close();
}

bool SearchInfo::CheckDivergence(SearchNode *node, State *state) {
  if (node->enabled_.size() != state->enabled()->size())
    return false;

  for (ActionInfo::Map::iterator it = node->enabled_.begin();
       it != node->enabled_.end(); ++it) {
    Action::Map::iterator mit = state->enabled()->find(it->first);
    if (mit == state->enabled()->end())
      return false;
    Action *action = mit->second;
    if (it->second->thd_ != action->thd())
      return false;
    if (it->second->obj_ != action->obj())
      return false;
    if (it->second->op_ != action->op())
      return false;
    if (it->second->inst_ != action->inst())
      return false;
  }

  // no divergence happens
  return true;
}

} // namespace systematic

