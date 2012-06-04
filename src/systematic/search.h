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

// File: systematic/search.h - The definitions for systematic interleaving
// search.

#ifndef SYSTEMATIC_SEARCH_H_
#define SYSTEMATIC_SEARCH_H_

#include "core/basictypes.h"
#include "systematic/program.h"
#include "systematic/search.pb.h"

namespace systematic {

// forward declaration
class SearchInfo;

// define action info which is used for divergence check
class ActionInfo {
 public:
  typedef std::map<Thread *, ActionInfo *> Map;

  std::string ToString();

 protected:
  ActionInfo()
      : thd_(NULL),
        obj_(NULL),
        op_(OP_INVALID),
        inst_(NULL) {}

  explicit ActionInfo(Action *action)
      : thd_(action->thd()),
        obj_(action->obj()),
        op_(action->op()),
        inst_(action->inst()) {}

  ~ActionInfo() {}

  Thread *thd_;
  Object *obj_;
  Operation op_;
  Inst *inst_;

 private:
  friend class SearchInfo;

  DISALLOW_COPY_CONSTRUCTORS(ActionInfo);
};

// define a node in the search stack
class SearchNode {
 public:
  typedef std::vector<SearchNode *> Vec;

  bool IsBacktrack(Thread *thd);
  bool IsDone(Thread *thd);
  void AddDone(Thread *thd);
  void AddBacktrack(Thread *thd);
  SearchNode *Prev();
  SearchNode *Next();
  bool Finished();
  std::string ToString();

  Thread *sel() { return sel_; }
  size_t idx() { return idx_; }
  void set_sel(Thread *thd) { sel_ = thd; }

 protected:
  SearchNode() : info_(NULL), idx_(0) {}
  ~SearchNode() {}

  SearchInfo *info_;
  size_t idx_; // the idx in the search stack
  Thread *sel_;
  Thread::Set backtrack_;
  Thread::Set done_;
  ActionInfo::Map enabled_; // used for divergence check

 private:
  friend class SearchInfo;

  DISALLOW_COPY_CONSTRUCTORS(SearchNode);
};

// define the search info (dfs search)
class SearchInfo {
 public:
  SearchInfo() : done_(false), num_runs_(0), cursor_(0) {}
  ~SearchInfo() {}

  bool Done() { return done_; }
  size_t StackSize() { return stack_.size(); }
  SearchNode *GetNextNode(State *state); // create a new node if needed
  SearchNode *Prev(SearchNode *node);
  SearchNode *Next(SearchNode *node);
  void UpdateForNext();
  void Load(const std::string &db_name, StaticInfo *sinfo, Program *program);
  void Save(const std::string &db_name, StaticInfo *sinfo, Program *program);

 protected:
  // helper functions
  bool CheckDivergence(SearchNode *node, State *state);

  bool done_;
  int num_runs_;
  SearchNode::Vec stack_;
  size_t cursor_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(SearchInfo);
};

// inline functions
inline bool SearchNode::IsBacktrack(Thread *thd) {
  return backtrack_.find(thd) != backtrack_.end();
}

inline bool SearchNode::IsDone(Thread *thd) {
  return done_.find(thd) != done_.end();
}

inline void SearchNode::AddDone(Thread *thd) {
  done_.insert(thd);
}

inline void SearchNode::AddBacktrack(Thread *thd) {
  backtrack_.insert(thd);
}

inline SearchNode *SearchNode::Prev() {
  return info_->Prev(this);
}

inline SearchNode *SearchNode::Next() {
  return info_->Next(this);
}

} // namespace systematic

#endif

