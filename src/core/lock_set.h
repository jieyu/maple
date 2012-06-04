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

// File: core/lock_set.h - Define lock set.

#ifndef CORE_LOCK_SET_H_
#define CORE_LOCK_SET_H_

#include <map>

#include "core/basictypes.h"
#include "core/atomic.h"

// Lock set.
class LockSet {
 public:
  typedef uint64 lock_version_t;

  LockSet() {}
  ~LockSet() {}

  bool Empty() { return set_.empty(); }
  void Add(address_t addr) { set_[addr] = GetNextLockVersion(); }
  void Remove(address_t addr) { set_.erase(addr); }
  bool Exist(address_t addr) { return set_.find(addr) != set_.end(); }
  bool Exist(address_t addr, lock_version_t version);
  void Clear() { set_.clear(); }
  bool Match(LockSet *ls);
  bool Disjoint(LockSet *ls);
  bool Disjoint(LockSet *rmt_ls1, LockSet *rmt_ls2);
  std::string ToString();
  void IterBegin() { it_ = set_.begin(); }
  bool IterEnd() { return it_ == set_.end(); }
  void IterNext() { ++it_; }
  address_t IterCurrAddr() { return it_->first; }
  lock_version_t IterCurrVersion() { return it_->second; }

 protected:
  typedef std::map<address_t, lock_version_t> LockVersionMap;

  static lock_version_t GetNextLockVersion() {
    return ATOMIC_ADD_AND_FETCH(&curr_lock_version_, 1);
  }

  LockVersionMap set_;
  LockVersionMap::iterator it_;

  // static data
  static lock_version_t curr_lock_version_;

  // using default copy constructor and assignment operator
};

#endif

