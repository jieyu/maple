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

// File: idiom/memo.cc - Implementation of memoization database.

#include "idiom/memo.h"

#include <cassert>
#include <algorithm>
#include "core/logging.h"

namespace idiom {

#define DEFAULT_FAILED_LIMIT         2
#define DEFAULT_TOTAL_FAILED_LIMIT   6

Memo::Memo(Mutex *lock, iRootDB *iroot_db)
    : internal_lock_(lock),
      iroot_db_(iroot_db),
      failed_limit_(DEFAULT_FAILED_LIMIT),
      total_failed_limit_(DEFAULT_TOTAL_FAILED_LIMIT) {
  // empty
}

Memo::~Memo() {
  // empty
}

iRoot *Memo::ChooseForTest() {
  // choose an iroot to test (no idiom is specified)
  // need to consider the relative idiom priority
  iRoot *candidate = NULL;
  // define idiom priority
  IdiomType idiom_prio[5] =
      {IDIOM_1, IDIOM_2, IDIOM_3, IDIOM_4, IDIOM_5};

  for (int i = 0; i < 5; i++) {
    IdiomType idiom = idiom_prio[i];
    candidate = ChooseForTest(idiom);
    if (candidate)
      return candidate;
  }
  return candidate;
}

iRoot *Memo::ChooseForTest(IdiomType idiom) {
  // choose an iroot to test for a given idiom
  iRootInfo *candidate = NULL;

  // step 1: find all iroots for the given idiom, and sort them
  // according to their iroot id
  std::map<iroot_id_t, iRootInfo *> candidates;
  for (CandidateMap::iterator it = candidate_map_.begin();
       it != candidate_map_.end(); ++it) {
    iRootInfo *iroot_info = it->first;
    iRoot *iroot = iroot_info->iroot();
    if (iroot->idiom() == idiom) {
      candidates[iroot->id()] = iroot_info;
    }
  }

  // step 2: find iroots from the application (not from common libs)
  // choose the first iroot that has the smallest number of test runs
  for (std::map<iroot_id_t, iRootInfo *>::iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    iRootInfo *iroot_info = it->second;
    if (!iroot_info->iroot()->HasCommonLibEvent()) {
      if (!candidate) {
        candidate = iroot_info;
      } else {
        if (iroot_info->total_test_runs() < candidate->total_test_runs())
          candidate = iroot_info;
        }
      }
  }
  if (candidate)
    return candidate->iroot();

  // step 3: find iroots from the rest
  for (std::map<iroot_id_t, iRootInfo *>::iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    iRootInfo *iroot_info = it->second;
    if (!candidate) {
      candidate = iroot_info;
    } else {
      if (iroot_info->total_test_runs() < candidate->total_test_runs()) {
        candidate = iroot_info;
      }
    }
  }
  if (candidate)
    return candidate->iroot();

  // no iroot can be tested, return NULL
  return NULL;
}

iRoot *Memo::ChooseForTest(iroot_id_t iroot_id) {
  iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
  DEBUG_ASSERT(iroot);
  return iroot;
}

void Memo::TestSuccess(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  CandidateMap::iterator cit = candidate_map_.find(iroot_info);
  DEBUG_ASSERT(cit != candidate_map_.end());
  // increment count
  cit->second++;
  iroot_info->set_total_test_runs(iroot_info->total_test_runs() + 1);
  // added to exposed set
  exposed_set_.insert(iroot_info);
}

void Memo::TestFail(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  CandidateMap::iterator cit = candidate_map_.find(iroot_info);
  DEBUG_ASSERT(cit != candidate_map_.end());
  // increment count
  cit->second++;
  iroot_info->set_total_test_runs(iroot_info->total_test_runs() + 1);
  // added to failed set if needed
  if (iroot_info->total_test_runs() >= total_failed_limit_) {
    failed_set_.insert(iroot_info);
  }
}

void Memo::Predicted(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  if (predicted_set_.find(iroot_info) == predicted_set_.end()) {
    // add to candidate set if it is newly predicted
    predicted_set_.insert(iroot_info);
    DEBUG_ASSERT(candidate_map_.find(iroot_info) == candidate_map_.end());
    candidate_map_[iroot_info] = 0;
  }
}

void Memo::Observed(iRoot *iroot, bool shadow, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  if (shadow)
    shadow_exposed_set_.insert(iroot_info);
  else
    exposed_set_.insert(iroot_info);
}

int Memo::TotalTestRuns(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);
  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  return iroot_info->total_test_runs();
}

bool Memo::Async(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  return iroot_info->async();
}

void Memo::SetAsync(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = GetiRootInfo(iroot, false);
  DEBUG_ASSERT(iroot_info);
  iroot_info->set_async(true);
}

size_t Memo::TotalCandidate(bool locking) {
  ScopedLock locker(internal_lock_, locking);

  return candidate_map_.size();
}

size_t Memo::TotalExposed(IdiomType idiom, bool shadow, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfoSet result;
  for (iRootInfoSet::iterator it = exposed_set_.begin();
       it != exposed_set_.end(); ++it) {
    if ((*it)->iroot()->idiom() == idiom) {
      result.insert(*it);
    }
  }

  if (shadow) {
    for (iRootInfoSet::iterator it = shadow_exposed_set_.begin();
         it != shadow_exposed_set_.end(); ++it) {
      if ((*it)->iroot()->idiom() == idiom) {
        result.insert(*it);
      }
    }
  }

  return result.size();
}

size_t Memo::TotalPredicted(bool locking) {
  ScopedLock locker(internal_lock_, locking);

  return predicted_set_.size();
}

void Memo::Merge(Memo *other) {
  // Merge iroot_info_map_.
  for (iRootInfoMap::iterator it = other->iroot_info_map_.begin();
       it != other->iroot_info_map_.end(); ++it) {
    iRoot *other_iroot = it->first;
    iRootInfo *other_iroot_info = it->second;
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    if (fit == iroot_info_map_.end()) {
      iRootInfo *iroot_info = GetiRootInfo(other_iroot, false);
      iroot_info->set_total_test_runs(other_iroot_info->total_test_runs());
      if (other_iroot_info->has_async()) {
        iroot_info->set_async(other_iroot_info->async());
      }
      iroot_info_map_[other_iroot] = iroot_info;
    } else {
      iRootInfo *iroot_info = fit->second;
      if (other_iroot_info->total_test_runs() > iroot_info->total_test_runs()) {
        iroot_info->set_total_test_runs(other_iroot_info->total_test_runs());
      }
      if (other_iroot_info->has_async() && other_iroot_info->async()) {
        iroot_info->set_async(true);
      }
    }
  }

  // Merge exposed_set_.
  for (iRootInfoSet::iterator it = other->exposed_set_.begin();
       it != other->exposed_set_.end(); ++it) {
    iRootInfo *other_iroot_info = *it;
    iRoot *other_iroot = other_iroot_info->iroot();
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    assert(fit != iroot_info_map_.end());
    exposed_set_.insert(fit->second);
  }

  // Merge failed_set_.
  for (iRootInfoSet::iterator it = other->failed_set_.begin();
       it != other->failed_set_.end(); ++it) {
    iRootInfo *other_iroot_info = *it;
    iRoot *other_iroot = other_iroot_info->iroot();
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    assert(fit != iroot_info_map_.end());
    failed_set_.insert(fit->second);
  }

  // Merge predicted_set_.
  for (iRootInfoSet::iterator it = other->predicted_set_.begin();
       it != other->predicted_set_.end(); ++it) {
    iRootInfo *other_iroot_info = *it;
    iRoot *other_iroot = other_iroot_info->iroot();
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    assert(fit != iroot_info_map_.end());
    predicted_set_.insert(fit->second);
  }

  // Merge shadow_exposed_set_.
  for (iRootInfoSet::iterator it = other->shadow_exposed_set_.begin();
       it != other->shadow_exposed_set_.end(); ++it) {
    iRootInfo *other_iroot_info = *it;
    iRoot *other_iroot = other_iroot_info->iroot();
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    assert(fit != iroot_info_map_.end());
    shadow_exposed_set_.insert(fit->second);
  }

  // Merge candidate_map_.
  for (CandidateMap::iterator it = other->candidate_map_.begin();
       it != other->candidate_map_.end(); ++it) {
    iRootInfo *other_iroot_info = it->first;
    iRoot *other_iroot = other_iroot_info->iroot();
    int other_test_runs = it->second;
    iRootInfoMap::iterator fit = iroot_info_map_.find(other_iroot);
    assert(fit != iroot_info_map_.end());
    iRootInfo *iroot_info = fit->second;
    CandidateMap::iterator cit = candidate_map_.find(iroot_info);
    if (cit == candidate_map_.end()) {
      candidate_map_[iroot_info] = other_test_runs;
    } else {
      if (other_test_runs > cit->second) {
        cit->second = other_test_runs;
      }
    }
  }
}

void Memo::RefineCandidate(bool memo_failed) {
  iRootInfoSet to_remove;

  // remove those candidates that reach failed limit
  for (CandidateMap::iterator it = candidate_map_.begin();
       it != candidate_map_.end(); ++it) {
    if (it->second >= failed_limit_) {
      to_remove.insert(it->first);
    }
  }
  for (iRootInfoSet::iterator it = to_remove.begin();
       it != to_remove.end(); ++it) {
    candidate_map_.erase(*it);
  }

  // remove those candidates that are exposed
  for (iRootInfoSet::iterator it = exposed_set_.begin();
       it != exposed_set_.end(); ++it) {
    candidate_map_.erase(*it);
  }

  // remove failed from candidates if needed
  if (memo_failed) {
    for (iRootInfoSet::iterator it = failed_set_.begin();
         it != failed_set_.end(); ++it) {
      candidate_map_.erase(*it);
    }
  }
}

void Memo::SampleCandidate(IdiomType idiom, size_t num) {
  // Find all the iroot info that match the given idiom.
  std::vector<iRootInfo *> iroot_info_vec;
  for (CandidateMap::iterator it = candidate_map_.begin();
       it != candidate_map_.end(); it++) {
    iRootInfo *iroot_info = it->first;
    if (iroot_info->iroot()->idiom() == idiom) {
      iroot_info_vec.push_back(iroot_info);
    }
  }

  // Randomly shuffle.
  random_shuffle(iroot_info_vec.begin(), iroot_info_vec.end());

  // Sample!
  if (num < iroot_info_vec.size()) {
    size_t remove_size = iroot_info_vec.size() - num;
    for (size_t i = 0; i < remove_size; i++) {
      candidate_map_.erase(iroot_info_vec[i]);
    }
  }
}

void Memo::Load(const std::string &db_name, StaticInfo *sinfo) {
  std::fstream in;
  in.open(db_name.c_str(), std::ios::in | std::ios::binary);
  proto_.ParseFromIstream(&in);
  in.close();
  // setup iroot info map
  for (int i = 0; i < proto_.iroot_info_size(); i++) {
    iRootInfoProto *iroot_info_proto = proto_.mutable_iroot_info(i);
    iroot_id_t iroot_id = iroot_info_proto->iroot_id();
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = new iRootInfo(iroot, iroot_info_proto);
    iroot_info_map_[iroot] = iroot_info;
  }
  // setup exposed set
  for (int i = 0; i < proto_.exposed_size(); i++) {
    iroot_id_t iroot_id = proto_.exposed(i);
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = FindiRootInfo(iroot, false);
    DEBUG_ASSERT(iroot_info);
    exposed_set_.insert(iroot_info);
  }
  // setup failed set
  for (int i = 0; i < proto_.failed_size(); i++) {
    iroot_id_t iroot_id = proto_.failed(i);
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = FindiRootInfo(iroot, false);
    DEBUG_ASSERT(iroot_info);
    failed_set_.insert(iroot_info);
  }
  // setup predicted set
  for (int i = 0; i < proto_.predicted_size(); i++) {
    iroot_id_t iroot_id = proto_.predicted(i);
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = FindiRootInfo(iroot, false);
    DEBUG_ASSERT(iroot_info);
    predicted_set_.insert(iroot_info);
  }
  // setup shadow exposed set
  for (int i = 0; i < proto_.shadow_exposed_size(); i++) {
    iroot_id_t iroot_id = proto_.shadow_exposed(i);
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = FindiRootInfo(iroot, false);
    DEBUG_ASSERT(iroot_info);
    shadow_exposed_set_.insert(iroot_info);
  }
  // setup candidate set
  for (int i = 0; i < proto_.candidate_size(); i++) {
    CandidateProto *candidate_proto = proto_.mutable_candidate(i);
    iroot_id_t iroot_id = candidate_proto->iroot_id();
    int test_runs = candidate_proto->test_runs();
    iRoot *iroot = iroot_db_->FindiRoot(iroot_id, false);
    DEBUG_ASSERT(iroot);
    iRootInfo *iroot_info = FindiRootInfo(iroot, false);
    DEBUG_ASSERT(iroot_info);
    candidate_map_[iroot_info] = test_runs;
  }
}

void Memo::Save(const std::string &db_name, StaticInfo *sinfo) {
  // sync exposed set
  proto_.clear_exposed();
  for (iRootInfoSet::iterator it = exposed_set_.begin();
       it != exposed_set_.end(); ++it) {
    proto_.add_exposed((*it)->iroot()->id());
  }
  // sync failed set
  proto_.clear_failed();
  for (iRootInfoSet::iterator it = failed_set_.begin();
       it != failed_set_.end(); ++it) {
    proto_.add_failed((*it)->iroot()->id());
  }
  // sync predicted set
  proto_.clear_predicted();
  for (iRootInfoSet::iterator it = predicted_set_.begin();
       it != predicted_set_.end(); ++it) {
    proto_.add_predicted((*it)->iroot()->id());
  }
  // sync shadow exposed set
  proto_.clear_shadow_exposed();
  for (iRootInfoSet::iterator it = shadow_exposed_set_.begin();
       it != shadow_exposed_set_.end(); ++it) {
    proto_.add_shadow_exposed((*it)->iroot()->id());
  }
  // sync candidate set
  proto_.clear_candidate();
  for (CandidateMap::iterator it = candidate_map_.begin();
       it != candidate_map_.end(); ++it) {
    CandidateProto *candidate_proto = proto_.add_candidate();
    candidate_proto->set_iroot_id(it->first->iroot()->id());
    candidate_proto->set_test_runs(it->second);
  }

  std::fstream out;
  out.open(db_name.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
  proto_.SerializeToOstream(&out);
  out.close();
}

iRootInfo *Memo::GetiRootInfo(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfo *iroot_info = FindiRootInfo(iroot, false);
  if (!iroot_info)
    iroot_info = CreateiRootInfo(iroot, false);
  return iroot_info;
}

iRootInfo *Memo::FindiRootInfo(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfoMap::iterator it = iroot_info_map_.find(iroot);
  if (it == iroot_info_map_.end())
    return NULL;
  else
    return it->second;
}

iRootInfo *Memo::CreateiRootInfo(iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootInfoProto *iroot_info_proto = proto_.add_iroot_info();
  iroot_info_proto->set_iroot_id(iroot->id());
  iroot_info_proto->set_total_test_runs(0);
  iRootInfo *iroot_info = new iRootInfo(iroot, iroot_info_proto);
  iroot_info_map_[iroot] = iroot_info;
  return iroot_info;
}

} // namespace idiom

