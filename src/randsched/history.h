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

// File: randsched/history.h - Define the Random scheduler history.

#ifndef RANDSCHED_HISTORY_H_
#define RANDSCHED_HISTORY_H_

#include <vector>

#include "core/basictypes.h"
#include "randsched/history.pb.h" // protobuf head file

namespace randsched {

// Random scheduler history.
class History {
 public:
  History() {}
  ~History() {}

  bool Empty() { return table_proto_.history_size() == 0; }
  unsigned long AvgInstCount();
  unsigned long AvgNumThreads();
  void Update(unsigned long length, unsigned long num_threads);
  void Load(const std::string &file_name);
  void Save(const std::string &file_name);

 private:
  HistoryTableProto table_proto_;

  DISALLOW_COPY_CONSTRUCTORS(History);
};

} // namespace randsched

#endif

