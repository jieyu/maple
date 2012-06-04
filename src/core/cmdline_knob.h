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

// File: core/cmdline_knob.h - Define command line switches.

#ifndef CORE_CMDLINE_KNOB_H_
#define CORE_CMDLINE_KNOB_H_

#include <map>

#include "core/basictypes.h"
#include "core/knob.h"

// Command line knob.
class CmdlineKnob : public Knob {
 public:
  CmdlineKnob();
  ~CmdlineKnob();

  void Parse(int argc, char *argv[]);
  void RegisterBool(const std::string &name, const std::string &desc,
                    const std::string &val);
  void RegisterInt(const std::string &name, const std::string &desc,
                   const std::string &val);
  void RegisterStr(const std::string &name, const std::string &desc,
                   const std::string &val);
  bool ValueBool(const std::string &name);
  int ValueInt(const std::string &name);
  std::string ValueStr(const std::string &name);

 private:
  typedef enum {
    KNOB_TYPE_INVALID = 0,
    KNOB_TYPE_BOOL,
    KNOB_TYPE_INT,
    KNOB_TYPE_STR,
  } KnobType;
  typedef std::pair<KnobType, void *> TypedKnob;
  typedef std::map<std::string, TypedKnob> KnobNameMap;

  bool Exist(const std::string &name);

  KnobNameMap knob_table_;

  DISALLOW_COPY_CONSTRUCTORS(CmdlineKnob);
};

#endif

