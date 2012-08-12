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

// File: idiom/memo_tool.cc - Implement the memoization command line tool.

#include "idiom/memo_tool.h"

#include <cstdlib>

namespace idiom {

MemoTool::MemoTool()
    : iroot_db_(NULL),
      memo_(NULL) {
  // Empty.
}

void MemoTool::HandlePreSetup() {
  OfflineTool::HandlePreSetup();

  knob_->RegisterStr("iroot_in", "the input iroot database path", "iroot.db");
  knob_->RegisterStr("iroot_out", "the output iroot database path", "iroot.db");
  knob_->RegisterStr("memo_in", "the input memoization database path", "memo.db");
  knob_->RegisterStr("memo_out", "the output memoization database path", "memo.db");
  knob_->RegisterStr("operation", "the operation to perform", "list");
  knob_->RegisterStr("arg", "the argument to the operation", "null");
  knob_->RegisterStr("path", "the path argument to the operation", "null");
  knob_->RegisterInt("num", "the integer argument to the operation", "0");
}

void MemoTool::HandlePostSetup() {
  OfflineTool::HandlePostSetup();

  // Load the iroot database.
  iroot_db_ = new iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);
  // load the memoization database.
  memo_ = new Memo(CreateMutex(), iroot_db_);
  memo_->Load(knob_->ValueStr("memo_in"), sinfo_);

  // Register operations.
  Register("list", std::tr1::bind(&MemoTool::list, this));
  Register("has_candidate", std::tr1::bind(&MemoTool::has_candidate, this));
  Register("sample_candidate", std::tr1::bind(&MemoTool::sample_candidate, this));
  Register("total_candidate", std::tr1::bind(&MemoTool::total_candidate, this));
  Register("total_exposed", std::tr1::bind(&MemoTool::total_exposed, this));
  Register("total_predicted", std::tr1::bind(&MemoTool::total_predicted, this));
  Register("apply", std::tr1::bind(&MemoTool::apply, this));
}

void MemoTool::HandleStart() {
  OfflineTool::HandleStart();

  // Dispatch the operation.
  Dispatch();
}

void MemoTool::HandleExit() {
  OfflineTool::HandleExit();

  if (!read_only_) {
    // Save the iroot database if needed.
    iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
    // Save the memoization database if needed.
    memo_->Save(knob_->ValueStr("memo_out"), sinfo_);
  }
}

void MemoTool::Register(const std::string& name,
                        const std::tr1::function<void(void)>& func) {
  operations_[name].name = name;
  operations_[name].func = func;
}

void MemoTool::Dispatch() {
  std::string operation = knob_->ValueStr("operation");
  if (operations_.find(operation) == operations_.end()) {
    printf("Operation \"%s\" is not found!\n", operation.c_str());
    read_only_ = true;
    return;
  }

  operations_[operation].func();
}

void MemoTool::list() {
  read_only_ = true;

  printf("Usage: memo_tool --operation=OP [options]\n\n");
  printf("Available operations:\n");
  for (std::map<std::string, Operation>::iterator it = operations_.begin();
       it != operations_.end(); ++it) {
    printf("  %s\n", it->first.c_str());
  }
}

void MemoTool::has_candidate() {
  read_only_ = true;

  iRoot *iroot = NULL;
  std::string arg = knob_->ValueStr("arg");
  if (arg == "0" || arg == "null") {
    iroot = memo_->ChooseForTest();
  } else if (arg == "1") {
    iroot = memo_->ChooseForTest(IDIOM_1);
  } else if (arg == "2") {
    iroot = memo_->ChooseForTest(IDIOM_2);
  } else if (arg == "3") {
    iroot = memo_->ChooseForTest(IDIOM_3);
  } else if (arg == "4") {
    iroot = memo_->ChooseForTest(IDIOM_4);
  } else if (arg == "5") {
    iroot = memo_->ChooseForTest(IDIOM_5);
  }

  if (iroot == NULL) {
    printf("0\n");
  } else {
    printf("1\n");
  }
}

void MemoTool::sample_candidate() {
  std::string arg = knob_->ValueStr("arg");
  int num = knob_->ValueInt("num");

  if (arg == "1") {
    memo_->SampleCandidate(IDIOM_1, num);
  } else if (arg == "2") {
    memo_->SampleCandidate(IDIOM_2, num);
  } else if (arg == "3") {
    memo_->SampleCandidate(IDIOM_3, num);
  } else if (arg == "4") {
    memo_->SampleCandidate(IDIOM_4, num);
  } else if (arg == "5") {
    memo_->SampleCandidate(IDIOM_5, num);
  } else {
    printf("Please specify an idiom\n");
    exit(1);
  }
}

void MemoTool::total_candidate() {
  read_only_ = true;

  size_t total = memo_->TotalCandidate(false);

  printf("%lu\n", total);
}

void MemoTool::total_exposed() {
  read_only_ = true;

  size_t idiom1 = memo_->TotalExposed(IDIOM_1, true, false);
  size_t idiom2 = memo_->TotalExposed(IDIOM_2, true, false);
  size_t idiom3 = memo_->TotalExposed(IDIOM_3, true, false);
  size_t idiom4 = memo_->TotalExposed(IDIOM_4, true, false);
  size_t idiom5 = memo_->TotalExposed(IDIOM_5, true, false);

  printf("%lu %lu %lu %lu %lu\n", idiom1, idiom2, idiom3, idiom4, idiom5);
}

void MemoTool::total_predicted() {
  read_only_ = true;

  printf("%lu\n", memo_->TotalPredicted(false));
}

void MemoTool::apply() {
  Memo *memo_other = new Memo(CreateMutex(), iroot_db_);
  memo_other->Load(knob_->ValueStr("path"), sinfo_);
  memo_->Merge(memo_other);
  memo_->RefineCandidate(true);
}

} // namespace idiom {

