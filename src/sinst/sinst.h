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

// File: sinst/sinst.h - Define the shared instruction database.

#ifndef SINST_SINST_H_
#define SINST_SINST_H_

#include <tr1/unordered_set>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/static_info.h"
#include "sinst/sinst.pb.h" // protobuf head file

namespace sinst {

// Shared instruction database
class SharedInstDB {
 public:
  SharedInstDB(Mutex *lock) : internal_lock_(lock) {}
  ~SharedInstDB() { delete internal_lock_; }

  bool Shared(Inst *inst) { return Shared(inst, true); }
  void SetShared(Inst *inst) { SetShared(inst, true); }
  bool Shared(Inst *inst, bool locking);
  void SetShared(Inst *inst, bool locking);
  void Load(const std::string &db_name, StaticInfo *sinfo);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 private:
  typedef std::tr1::unordered_set<Inst *> SharedInstSet;

  Mutex *internal_lock_;
  SharedInstSet shared_inst_set_;
  SharedInstTableProto table_proto_;

  DISALLOW_COPY_CONSTRUCTORS(SharedInstDB);
};

} // namespace sinst

#endif

