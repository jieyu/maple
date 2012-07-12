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

// File: core/pin_util.hpp - Implementation of PIN utility functions.

#include "core/pin_util.hpp"

#include "core/logging.h"

RTN FindRTN(IMG img, const std::string &func_name) {
  RTN rtn = RTN_FindByName(img, func_name.c_str());
  if (RTN_Valid(rtn))
    return rtn;

  // handle those symbols with version numbers.
  // e.g. pthread_create has global name: pthread_create@@GLIBC...
  std::string func_name_v(func_name);
  func_name_v.append("@@");
  for(SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {
    if (SYM_Name(sym).find(func_name_v) != std::string::npos) {
      RTN rtn = RTN_FindByAddress(SYM_Address(sym));
      DEBUG_ASSERT(RTN_Valid(rtn));
      return rtn;
    }
  }

  return RTN_Invalid();
}

IMG GetImgByTrace(TRACE trace) {
  IMG img = IMG_Invalid();
  RTN rtn = TRACE_Rtn(trace);
  if (RTN_Valid(rtn)) {
    SEC sec = RTN_Sec(rtn);
    if (SEC_Valid(sec)) {
      img = SEC_Img(sec);
    }
  }
  return img;
}

bool BBLContainMemOp(BBL bbl) {
  for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
    if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
      continue;
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
      return true;
  }
  return false;
}

