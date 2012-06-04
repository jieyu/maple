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

// File: pct/history.cc - Implementation of the PCT scheduler history.

#include "pct/history.h"

#include <fstream>

namespace pct {

unsigned long History::AvgInstCount() {
  double total = 0.0;
  int size = table_proto_.history_size();
  for (int i = 0; i < size; i++) {
    total += (double)table_proto_.history(i).inst_count();
  }
  return (unsigned long)(total / (double)size);
}

unsigned long History::AvgNumThreads() {
  double total = 0.0;
  int size = table_proto_.history_size();
  for (int i = 0; i < size; i++) {
    total += (double)table_proto_.history(i).num_threads();
  }
  return (unsigned long)(total / (double)size);
}

void History::Update(unsigned long inst_count, unsigned long num_threads) {
  HistoryProto *proto = table_proto_.add_history();
  proto->set_inst_count(inst_count);
  proto->set_num_threads(num_threads);
}

void History::Load(const std::string &file_name) {
  std::fstream in(file_name.c_str(), std::ios::in | std::ios::binary);
  table_proto_.ParseFromIstream(&in);
  in.close();
}

void History::Save(const std::string &file_name) {
  std::fstream out(file_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  table_proto_.SerializeToOstream(&out);
  out.close();
}

} // namespace pct

