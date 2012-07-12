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

// File: race/profiler.cpp - Implementation of the profiler for data
// race detectors.

#include "race/profiler.hpp"

namespace race {

void Profiler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("ignore_lib", "whether ignore accesses from common libraries", "0");
  knob_->RegisterStr("race_in", "the input race database path", "race.db");
  knob_->RegisterStr("race_out", "the output race database path", "race.db");

  djit_analyzer_ = new Djit;
  djit_analyzer_->Register();
}

void Profiler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // load race db
  race_db_ = new RaceDB(CreateMutex());
  race_db_->Load(knob_->ValueStr("race_in"), sinfo_);

  // add data race detector
  if (djit_analyzer_->Enabled()) {
    djit_analyzer_->Setup(CreateMutex(), race_db_);
    AddAnalyzer(djit_analyzer_);
  }

  // make sure that we use one data race detector
  if (!djit_analyzer_)
    Abort("please choose a data race detector\n");
}

bool Profiler::HandleIgnoreMemAccess(IMG img) {
  if (!IMG_Valid(img))
    return true;
  Image *image = sinfo_->FindImage(IMG_Name(img));
  DEBUG_ASSERT(image);
  if (image->IsPthread())
    return true;
  if (knob_->ValueBool("ignore_lib")) {
    if (image->IsCommonLib())
      return true;
  }
  return false;
}

void Profiler::HandleProgramExit() {
  ExecutionControl::HandleProgramExit();

  // save race db
  race_db_->Save(knob_->ValueStr("race_out"), sinfo_);
}

} // namespace race

