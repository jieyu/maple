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

// File: core/lock_set.cc - Implementation of lock set.

#include "core/lock_set.h"

#include <sstream>

LockSet::lock_version_t LockSet::curr_lock_version_ = 0;

bool LockSet::Exist(address_t addr, lock_version_t version) {
  LockVersionMap::iterator it = set_.find(addr);
  if (it != set_.end()) {
    if (it->second == version)
      return true;
  }
  return false;
}

bool LockSet::Match(LockSet *ls) {
  // return true if this and ls have the same set of locks
  if (set_.size() != ls->set_.size())
    return false;

  for (LockVersionMap::iterator it = set_.begin(); it != set_.end(); ++it) {
    LockVersionMap::iterator mit = ls->set_.find(it->first);
    if (mit == ls->set_.end())
      return false;
  }
  return true;
}

bool LockSet::Disjoint(LockSet *ls) {
  for (LockVersionMap::iterator it = set_.begin(); it != set_.end(); ++it) {
    LockVersionMap::iterator mit = ls->set_.find(it->first);
    if (mit != ls->set_.end())
      return false;
  }
  return true;
}

bool LockSet::Disjoint(LockSet *rmt_ls1, LockSet *rmt_ls2) {
  for (LockVersionMap::iterator it = set_.begin(); it != set_.end(); ++it) {
    LockVersionMap::iterator mit1 = rmt_ls1->set_.find(it->first);
    LockVersionMap::iterator mit2 = rmt_ls2->set_.find(it->first);
    if (mit1 != rmt_ls1->set_.end() && mit2 != rmt_ls2->set_.end()) {
      // compare the version number between two remote lock sets
      if (mit1->second == mit2->second)
        return false;
    }
  }
  return true;
}

std::string LockSet::ToString() {
  std::stringstream ss;
  ss << "[";
  for (LockVersionMap::iterator it = set_.begin(); it != set_.end(); ++it) {
    ss << std::hex << "0x" << it->first << " ";
  }
  ss << "]";
  return ss.str();
}

