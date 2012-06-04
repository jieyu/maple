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

// File: core/cmdline_knob.cc - Implementation of the command line
// switches.

#include "core/cmdline_knob.h"

#include <getopt.h>
#include "core/logging.h"

CmdlineKnob::CmdlineKnob() {
  // empty
}

CmdlineKnob::~CmdlineKnob() {
  // empty
}

bool CmdlineKnob::Exist(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  if (it == knob_table_.end())
    return false;
  else
    return true;
}

void CmdlineKnob::Parse(int argc, char *argv[]) {
  // create long options and optstring
  struct option *long_options = new struct option[knob_table_.size() + 1];
  std::string optstring;
  std::map<int, TypedKnob *> opt_map;
  int idx = 0;
  for (KnobNameMap::iterator it = knob_table_.begin();
       it != knob_table_.end(); ++it, ++idx) {
    long_options[idx].name = it->first.c_str();
    long_options[idx].has_arg = optional_argument;
    long_options[idx].flag = NULL;
    long_options[idx].val = 'a' + idx;
    optstring.append(1, (char)long_options[idx].val);
    optstring.append(":");
    opt_map[long_options[idx].val] = &it->second;
  }
  long_options[idx].name = NULL;
  long_options[idx].has_arg = 0;
  long_options[idx].flag = NULL;
  long_options[idx].val = 0;

  // start parsing
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, optstring.c_str(),
                        long_options, &option_index);
    if (c == -1)
      break;

    TypedKnob *knob = opt_map[c];
    DEBUG_ASSERT(knob);

    if (knob->first == KNOB_TYPE_BOOL) {
      *((bool *)knob->second) = atoi(optarg) ? true : false;
    } else if (knob->first == KNOB_TYPE_INT) {
      *((int *)knob->second) = atoi(optarg);
    } else if (knob->first == KNOB_TYPE_STR) {
      *((std::string *)knob->second) = std::string(optarg);
    } else {
      DEBUG_ASSERT(0); // impossible
    }
  }
}

void CmdlineKnob::RegisterBool(const std::string &name, const std::string &desc,
                               const std::string &val) {
  if (Exist(name))
    return;

  bool *value = new bool;
  if (atoi(val.c_str()))
    *value = true;
  else
    *value = false;
  knob_table_[name] = TypedKnob(KNOB_TYPE_BOOL, value);
}

void CmdlineKnob::RegisterInt(const std::string &name, const std::string &desc,
                              const std::string &val) {
  if (Exist(name))
    return;

  int *value = new int;
  *value = atoi(val.c_str());
  knob_table_[name] = TypedKnob(KNOB_TYPE_INT, value);
}

void CmdlineKnob::RegisterStr(const std::string &name, const std::string &desc,
                              const std::string &val) {
  if (Exist(name))
    return;

  std::string *value = new std::string(val);
  knob_table_[name] = TypedKnob(KNOB_TYPE_STR, value);
}

bool CmdlineKnob::ValueBool(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_BOOL);
  return *((bool *)it->second.second);
}

int CmdlineKnob::ValueInt(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_INT);
  return *((int *)it->second.second);
}

std::string CmdlineKnob::ValueStr(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_STR);
  return *((std::string *)it->second.second);
}

