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

// File: tracer/profiler.cpp - Implementation of the tracer online
// profiler.

#include "tracer/profiler.hpp"

#include "tracer/log.h"
#include "tracer/recorder.h"

namespace tracer {

void Profiler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("ignore_ic_pthread", "do not count instructions in pthread", "1");
  knob_->RegisterBool("ignore_lib", "whether ignore accesses from common libraries", "0");

  recorder_ = new RecorderAnalyzer;
  recorder_->Register();
}

void Profiler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // add record analyzer
  recorder_->Setup(CreateMutex());
  AddAnalyzer(recorder_);
}

bool Profiler::HandleIgnoreInstCount(IMG img) {
  if (knob_->ValueBool("ignore_ic_pthread")) {
    if (!IMG_Valid(img))
      return false;
    Image *image = sinfo_->FindImage(IMG_Name(img));
    DEBUG_ASSERT(image);
    if (image->IsPthread())
      return true;
  }
  return false;
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

} // namespace tracer

