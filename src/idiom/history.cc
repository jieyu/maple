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

// File: idiom/history.cc - Implementation of active testing history.

#include "idiom/history.h"

#include <fstream>

namespace idiom {

void TestHistory::CreateEntry(iRoot *iroot) {
  curr_proto_ = table_proto_.add_history();
  curr_proto_->set_iroot_id(iroot->id());
}

void TestHistory::UpdateSeed(unsigned int seed) {
  curr_proto_->set_seed(seed);
}

void TestHistory::UpdateResult(bool success) {
  curr_proto_->set_success(success);
}

int TestHistory::TotalTestRuns(iRoot *iroot) {
  int total_test_runs = 0;
  for (int i = 0; i < table_proto_.history_size(); i++) {
    HistoryProto *proto = table_proto_.mutable_history(i);
    if (proto->iroot_id() == iroot->id())
      total_test_runs++;
  }
  return total_test_runs;
}

void TestHistory::Load(const std::string &file_name) {
  std::fstream in(file_name.c_str(), std::ios::in | std::ios::binary);
  table_proto_.ParseFromIstream(&in);
  in.close();
}

void TestHistory::Save(const std::string &file_name) {
  std::fstream out(file_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  table_proto_.SerializeToOstream(&out);
  out.close();
}

} // namespace idiom

