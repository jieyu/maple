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

// File: core/offline_tool.h - The abstract definition of offline tools.

#ifndef CORE_OFFLINE_TOOL_H_
#define CORE_OFFLINE_TOOL_H_

#include "core/basictypes.h"
#include "core/logging.h"
#include "core/sync.h"
#include "core/cmdline_knob.h"
#include "core/static_info.h"

class OfflineTool {
 public:
  OfflineTool();
  virtual ~OfflineTool();

  void Initialize();
  void PreSetup();
  void PostSetup();
  void Parse(int argc, char *argv[]);
  void Start();
  void Exit();

 protected:
  virtual Mutex *CreateMutex() { return new NullMutex; }
  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandleStart();
  virtual void HandleExit();

  Mutex *kernel_lock_;
  Knob *knob_;
  LogFile *debug_file_;
  StaticInfo *sinfo_;
  bool read_only_; // Whether this tool is a read-only tool.

  static OfflineTool *tool_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(OfflineTool);
};

#endif

