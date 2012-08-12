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

// File: core/offline_tool.cc - The abstract implementation of offline
// tools.

#include "core/offline_tool.h"

OfflineTool *OfflineTool::tool_ = NULL;

OfflineTool::OfflineTool()
    : kernel_lock_(NULL),
      knob_(NULL),
      debug_file_(NULL),
      sinfo_(NULL),
      read_only_(false) {
  // empty
}

OfflineTool::~OfflineTool() {
  // empty
}

void OfflineTool::Initialize() {
  logging_init(CreateMutex());
  kernel_lock_ = CreateMutex();
  Knob::Initialize(new CmdlineKnob);
  knob_ = Knob::Get();
  tool_ = this;
}

void OfflineTool::PreSetup() {
  knob_->RegisterStr("debug_out", "the output file for the debug messages", "stdout");
  knob_->RegisterStr("sinfo_in", "the input static info database path", "sinfo.db");
  knob_->RegisterStr("sinfo_out", "the output static info database path", "sinfo.db");

  HandlePreSetup();
}

void OfflineTool::PostSetup() {
  // setup debug output
  if (knob_->ValueStr("debug_out") == "stderr") {
    debug_log->ResetLogFile();
    debug_log->RegisterLogFile(stderr_log_file);
  } else if (knob_->ValueStr("debug_out") == "stdout") {
    debug_log->ResetLogFile();
    debug_log->RegisterLogFile(stdout_log_file);
  } else {
    debug_file_ = new FileLogFile(knob_->ValueStr("debug_out"));
    debug_file_->Open();
    debug_log->ResetLogFile();
    debug_log->RegisterLogFile(debug_file_);
  }

  // load static info
  sinfo_ = new StaticInfo(CreateMutex());
  sinfo_->Load(knob_->ValueStr("sinfo_in"));

  HandlePostSetup();
}

void OfflineTool::Parse(int argc, char *argv[]) {
  ((CmdlineKnob *)knob_)->Parse(argc, argv);
}

void OfflineTool::Start() {
  HandleStart();
}

void OfflineTool::Exit() {
  HandleExit();

  if (!read_only_) {
    // Save static info.
    sinfo_->Save(knob_->ValueStr("sinfo_out"));
  }

  // close debug file if exists
  if (debug_file_)
    debug_file_->Close();

  // finalize logging
  logging_fini();
}

void OfflineTool::HandlePreSetup() {
  // empty
}

void OfflineTool::HandlePostSetup() {
  // empty
}

void OfflineTool::HandleStart() {
  // empty
}

void OfflineTool::HandleExit() {
  // empty
}

