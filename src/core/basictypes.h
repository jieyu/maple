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

// File: core/basictypes.h - Type definitions for the project.

#ifndef CORE_BASICTYPES_H_
#define CORE_BASICTYPES_H_

#include <stdint.h>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

typedef signed char   schar;
typedef unsigned char uchar;
typedef int8_t        int8;
typedef int16_t       int16;
typedef int32_t       int32;
typedef int64_t       int64;
typedef uint8_t       uint8;
typedef uint16_t      uint16;
typedef uint32_t      uint32;
typedef uint64_t      uint64;

typedef unsigned long address_t;
typedef uint64 timestamp_t;
typedef uint64 thread_id_t;
#define INVALID_ADDRESS static_cast<address_t>(-1)
#define INVALID_TIMESTAMP static_cast<timestamp_t>(-1)
#define INVALID_THD_ID static_cast<thread_id_t>(-1)

// Used to align addresses
#define WORD_SIZE sizeof(void *)
#define UNIT_MASK(unit_size) (~((unit_size)-1))
#define UNIT_DOWN_ALIGN(addr,unit_size) ((addr) & UNIT_MASK(unit_size))
#define UNIT_UP_ALIGN(addr,unit_size) \
    (((addr)+(unit_size)-1) & UNIT_MASK(unit_size))

// Used to calculate timestamp distances
#define TIME_DISTANCE(start,end) \
    ((end)>=(start) ? (end)-(start) : (((timestamp_t)0)-((start)-(end))))

// Disallow the copy constructor and operator= functions
#define DISALLOW_COPY_CONSTRUCTORS(TypeName)    \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)

#endif


