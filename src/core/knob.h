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

// File: core/knob.h - Define command line switches.

#ifndef CORE_KNOB_H_
#define CORE_KNOB_H_

#include "core/basictypes.h"

// The interface class for the command line switches.
class Knob {
 public:
  Knob() {}
  virtual ~Knob() {}

  virtual void RegisterBool(const std::string &name, const std::string &desc,
                            const std::string &val) = 0;
  virtual void RegisterInt(const std::string &name, const std::string &desc,
                           const std::string &val) = 0;
  virtual void RegisterStr(const std::string &name, const std::string &desc,
                           const std::string &val) = 0;
  virtual bool ValueBool(const std::string &name) = 0;
  virtual int ValueInt(const std::string &name) = 0;
  virtual std::string ValueStr(const std::string &name) = 0;

  static void Initialize(Knob *knob) { knob_ = knob; }
  static Knob *Get() { return knob_; }

 protected:
  static Knob *knob_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Knob);
};

#endif

