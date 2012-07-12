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

// File: sinst/profiler.cpp - Implementation of the controller for
// shared instruction profiler.

#include "sinst/profiler.hpp"

namespace sinst {

void Profiler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterStr("sinst_in", "the input shared inst database path", "sinst.db");
  knob_->RegisterStr("sinst_out", "the output shared inst database path", "sinst.db");

  sinst_analyzer_ = new SharedInstAnalyzer;
  sinst_analyzer_->Register();
}

void Profiler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // load shared inst db
  sinst_db_ = new SharedInstDB(CreateMutex());
  sinst_db_->Load(knob_->ValueStr("sinst_in"), sinfo_);
  // add sinst analyzer
  sinst_analyzer_->Setup(CreateMutex(), sinst_db_);
  AddAnalyzer(sinst_analyzer_);
}

void Profiler::HandleProgramExit() {
  ExecutionControl::HandleProgramExit();

  if (sinst_analyzer_->Enabled()) {
    // save shared inst db
    sinst_db_->Save(knob_->ValueStr("sinst_out"), sinfo_);
  }
}

} // namespace sinst

