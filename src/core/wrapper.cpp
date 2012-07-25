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

// File: core/wrapper.cpp - Implementation of function wrappers.

#include "core/wrapper.hpp"

// The singleton instance for WrapperFactory.
WrapperFactory *WrapperFactory::instance_ = NULL;

// Register wrappers.
REGISTER_WRAPPER(Malloc);
REGISTER_WRAPPER(Calloc);
REGISTER_WRAPPER(Realloc);
REGISTER_WRAPPER(Free);
REGISTER_WRAPPER(Valloc);

REGISTER_WRAPPER(Sleep);
REGISTER_WRAPPER(Usleep);

REGISTER_WRAPPER(SchedSetScheduler);
REGISTER_WRAPPER(SchedYield);
REGISTER_WRAPPER(SchedSetAffinity);
REGISTER_WRAPPER(SetPriority);

REGISTER_WRAPPER(PthreadCreate);
REGISTER_WRAPPER(PthreadJoin);
REGISTER_WRAPPER(PthreadMutexTryLock);
REGISTER_WRAPPER(PthreadMutexLock);
REGISTER_WRAPPER(PthreadMutexUnlock);
REGISTER_WRAPPER(PthreadCondSignal);
REGISTER_WRAPPER(PthreadCondBroadcast);
REGISTER_WRAPPER(PthreadCondWait);
REGISTER_WRAPPER(PthreadCondTimedwait);
REGISTER_WRAPPER(PthreadBarrierInit);
REGISTER_WRAPPER(PthreadBarrierWait);

