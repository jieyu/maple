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

// File: sinst/sinst.cc - Implementation of the shared instruction database.

#include "sinst/sinst.h"

#include <fstream>

#include "core/logging.h"

namespace sinst {

bool SharedInstDB::Shared(Inst *inst, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  SharedInstSet::iterator it = shared_inst_set_.find(inst);
  if (it == shared_inst_set_.end())
    return false;
  else
    return true;
}

void SharedInstDB::SetShared(Inst *inst, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  SharedInstSet::iterator it = shared_inst_set_.find(inst);
  if (it == shared_inst_set_.end()) {
    shared_inst_set_.insert(inst);
    SharedInstProto *proto = table_proto_.add_shared_inst();
    proto->set_inst_id(inst->id());
  }
}

void SharedInstDB::Load(const std::string &db_name, StaticInfo *sinfo) {
  std::fstream in(db_name.c_str(), std::ios::in | std::ios::binary);
  table_proto_.ParseFromIstream(&in);
  in.close();
  // setup shared inst set
  for (int i = 0; i < table_proto_.shared_inst_size(); i++) {
    const SharedInstProto &proto = table_proto_.shared_inst(i);
    Inst *inst = sinfo->FindInst(proto.inst_id());
    DEBUG_ASSERT(inst);
    shared_inst_set_.insert(inst);
  }
}

void SharedInstDB::Save(const std::string &db_name, StaticInfo *sinfo) {
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  table_proto_.SerializeToOstream(&out);
  out.close();
}

} // namespace sinst

