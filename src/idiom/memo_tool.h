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

// File: idiom/memo_tool.h - Define the memoization command line tool.

#ifndef IDIOM_MEMO_TOOL_H_
#define IDIOM_MEMO_TOOL_H_

#include <tr1/functional>

#include "core/basictypes.h"
#include "core/knob.h"
#include "core/offline_tool.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"

namespace idiom {

// We introduce this class mainly because the protobuf python binding is very
// slow compared to its C++ counterpart. This command line tool provides
// utilities for fast accessing memoization database.
class MemoTool : public OfflineTool {
 public:
  MemoTool();
  virtual ~MemoTool() {}

 protected:
  // Define an operation.
  struct Operation {
    std::string name;
    std::tr1::function<void(void)> func;
  };

  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandleStart();
  virtual void HandleExit();

  void Register(const std::string& name,
                const std::tr1::function<void(void)>& func);
  void Dispatch();

  // Belows are operation handling functions.
  void list();
  void has_candidate();
  void sample_candidate();
  void total_candidate();
  void total_exposed();
  void total_predicted();
  void apply();

  iRootDB *iroot_db_;
  Memo *memo_;
  std::map<std::string, Operation> operations_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(MemoTool);
};

} // namespace idiom {

#endif // IDIOM_MEMO_TOOL_H_

