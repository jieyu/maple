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

// File: idiom/history.h - Define active testing history.

#ifndef IDIOM_HISTORY_H_
#define IDIOM_HISTORY_H_

#include <vector>

#include "core/basictypes.h"
#include "idiom/iroot.h"
#include "idiom/history.pb.h"

namespace idiom {

// Active testing history
class TestHistory {
 public:
  TestHistory() : curr_proto_(NULL) {}
  ~TestHistory() {}

  void CreateEntry(iRoot *iroot);
  void UpdateSeed(unsigned int seed);
  void UpdateResult(bool success);
  int TotalTestRuns(iRoot *iroot);
  void Load(const std::string &file_name);
  void Save(const std::string &file_name);

 private:
  HistoryTableProto table_proto_;
  HistoryProto *curr_proto_;

  DISALLOW_COPY_CONSTRUCTORS(TestHistory);
};

} // namespace idiom

#endif
