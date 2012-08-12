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

// File: idiom/memo.h - Define memoization database.

#ifndef IDIOM_MEMO_H_
#define IDIOM_MEMO_H_

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/static_info.h"
#include "idiom/iroot.h"
#include "idiom/memo.pb.h"

namespace idiom {

class Memo;

class iRootInfo {
 public:
  iRoot *iroot() { return iroot_; }
  int total_test_runs() { return proto_->total_test_runs(); }
  bool async() { return (proto_->has_async() ? proto_->async() : false); }
  bool has_async() { return proto_->has_async(); }
  void set_total_test_runs(int n) { proto_->set_total_test_runs(n); }
  void set_async(bool async) { proto_->set_async(async); }

 protected:
  iRootInfo(iRoot *iroot, iRootInfoProto *proto)
      : iroot_(iroot), proto_(proto) {}
  ~iRootInfo() {}

  iRoot *iroot_;
  iRootInfoProto *proto_;

 private:
  friend class Memo;

  DISALLOW_COPY_CONSTRUCTORS(iRootInfo);
};

class Memo {
 public:
  typedef enum {
    LEVEL_INVALID = 0,
    LEVEL_EXPOSED_ONLY,
    LEVEL_EXPOSED_FAILED,
  } LevelType;

  Memo(Mutex *lock, iRootDB *iroot_db);
  ~Memo();

  iRoot *ChooseForTest();
  iRoot *ChooseForTest(IdiomType idiom);
  iRoot *ChooseForTest(iroot_id_t iroot_id);
  void TestSuccess(iRoot *iroot, bool locking);
  void TestFail(iRoot *iroot, bool locking);
  void Predicted(iRoot *iroot, bool locking);
  void Observed(iRoot *iroot, bool shadow, bool locking);
  int TotalTestRuns(iRoot *iroot, bool locking);
  bool Async(iRoot *iroot, bool locking);
  void SetAsync(iRoot *iroot, bool locking);
  size_t TotalCandidate(bool locking);
  size_t TotalExposed(IdiomType idiom, bool shadow, bool locking);
  size_t TotalPredicted(bool locking);
  void Merge(Memo *other);
  void RefineCandidate(bool memo_failed);
  void SampleCandidate(IdiomType idiom, size_t num);
  void Load(const std::string &db_name, StaticInfo *sinfo);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 protected:
  typedef std::tr1::unordered_map<iRoot *, iRootInfo *> iRootInfoMap;
  typedef std::tr1::unordered_set<iRootInfo *> iRootInfoSet;
  typedef std::tr1::unordered_map<iRootInfo *, int> CandidateMap;

  iRootInfo *GetiRootInfo(iRoot *iroot, bool locking);
  iRootInfo *FindiRootInfo(iRoot *iroot, bool locking);
  iRootInfo *CreateiRootInfo(iRoot *iroot, bool locking);

  Mutex *internal_lock_;
  iRootDB *iroot_db_;
  iRootInfoMap iroot_info_map_;
  iRootInfoSet exposed_set_;
  iRootInfoSet failed_set_;
  iRootInfoSet predicted_set_;
  iRootInfoSet shadow_exposed_set_; // optional
  CandidateMap candidate_map_;
  MemoProto proto_;
  int failed_limit_;
  int total_failed_limit_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Memo);
};

} // namespace idiom

#endif

