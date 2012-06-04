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

// File: core/pin_knob.cpp - Implementation of command line switches.

#include "core/pin_knob.hpp"

#include "core/logging.h"

bool PinKnob::Exist(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  if (it == knob_table_.end())
    return false;
  else
    return true;
}

void PinKnob::RegisterBool(const std::string &name, const std::string &desc,
                           const std::string &val) {
  if (Exist(name))
    return;

  KNOB<bool> *knob = new KNOB<bool>(KNOB_MODE_WRITEONCE, "pintool",
                                    name, val, desc);
  knob_table_[name] = TypedKnob(KNOB_TYPE_BOOL, knob);
}

void PinKnob::RegisterInt(const std::string &name, const std::string &desc,
                          const std::string &val) {
  if (Exist(name))
    return;

  KNOB<int> *knob = new KNOB<int>(KNOB_MODE_WRITEONCE, "pintool",
                                  name, val, desc);
  knob_table_[name] = TypedKnob(KNOB_TYPE_INT, knob);
}

void PinKnob::RegisterStr(const std::string &name, const std::string &desc,
                          const std::string &val) {
  if (Exist(name))
    return;

  KNOB<std::string> *knob = new KNOB<std::string>(KNOB_MODE_WRITEONCE,
                                                  "pintool", name, val, desc);
  knob_table_[name] = TypedKnob(KNOB_TYPE_STR, knob);
}

bool PinKnob::ValueBool(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_BOOL);
  return ((KNOB<bool> *)it->second.second)->Value();
}

int PinKnob::ValueInt(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_INT);
  return ((KNOB<int> *)it->second.second)->Value();
}

std::string PinKnob::ValueStr(const std::string &name) {
  KnobNameMap::iterator it = knob_table_.find(name);
  DEBUG_ASSERT(it != knob_table_.end() && it->second.first == KNOB_TYPE_STR);
  return ((KNOB<std::string> *)it->second.second)->Value();
}

