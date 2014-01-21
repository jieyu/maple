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

// File: core/pin_util.hpp - Define PIN utility functions.

#ifndef CORE_PIN_UTIL_HPP_
#define CORE_PIN_UTIL_HPP_

#include "pin.H"

#ifdef CONFIG_PINPLAY
#include "pinplay.H"
#endif

#include "core/basictypes.h"

// Setup call orders for PinPlay.
#ifdef CONFIG_PINPLAY
#define CALL_ORDER_BEFORE IARG_CALL_ORDER, (PINPLAY_ENGINE::PinPlayFirstBeforeCallOrder() - 1),
#define CALL_ORDER_AFTER IARG_CALL_ORDER, (PINPLAY_ENGINE::PinPlayLastAfterCallOrder() + 1),
#else
#define CALL_ORDER_BEFORE
#define CALL_ORDER_AFTER
#endif

// Global definitions

// Find RTN by function name in an image.
extern RTN FindRTN(IMG img, const std::string &func_name);

// Get the IMG that contains the TRACE.
extern IMG GetImgByTrace(TRACE trace);

// Return whether the given bbl contains non-stack memory access.
extern bool BBLContainMemOp(BBL bbl);

#endif

