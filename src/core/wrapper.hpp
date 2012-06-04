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

// File: core/wrapper.hpp - Define function wrappers.

#ifndef CORE_WRAPPER_HPP_
#define CORE_WRAPPER_HPP_

#include "pin.H"

#include "core/basictypes.h"
#include "core/logging.h"
#include "core/pin_util.hpp"

// Define the context when calling a wrapper function.
class WrapperContext {
 public:
  WrapperContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr)
      : tid_(tid), ctxt_(ctxt), ret_addr_(ret_addr) {}
  virtual ~WrapperContext() {}

  THREADID tid() { return tid_; }
  CONTEXT *ctxt() { return ctxt_; }
  ADDRINT ret_addr() { return ret_addr_; }

 protected:
  THREADID tid_;
  CONTEXT *ctxt_;
  ADDRINT ret_addr_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(WrapperContext);
};

// Define function wrapper.
template <typename C, typename W>
class Wrapper {
 public:
  typedef void (* HandlerType)(C *);

  static void Register(IMG img, const char *func_name, const char *lib_name);

 protected:
  Wrapper() {}
  virtual ~Wrapper() {}

  static const char *func_name_;
  static AFUNPTR ori_funptr_;
  static HandlerType handler_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Wrapper);
};

template <typename C, typename W>
const char *Wrapper<C,W>::func_name_ = "INVALID_FUNC_NAME";

template <typename C, typename W>
AFUNPTR Wrapper<C,W>::ori_funptr_ = NULL;

template <typename C, typename W>
void (* Wrapper<C,W>::handler_)(C *) = NULL;

template <typename C, typename W>
void Wrapper<C,W>::Register(IMG img, const char *func_name,
                            const char *lib_name) {
  if (IMG_Name(img).find(lib_name) == std::string::npos)
    return;

  RTN rtn = FindRTN(img, func_name);
  if (RTN_Valid(rtn)) {
    func_name_ = func_name;
    ori_funptr_ = RTN_Funptr(rtn);
    DEBUG_FMT_PRINT_SAFE("Register Wrapper '%s' in '%s'\n",
                         func_name, lib_name);
  }
}

#endif // end of #ifndef CORE_WRAPPER_HPP_

