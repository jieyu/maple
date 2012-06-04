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

// File: core/atomic.h - Define atomic operations.

#ifndef CORE_ATOMIC_H_
#define CORE_ATOMIC_H_

#include "core/basictypes.h"

// These builtins perform the operation suggested by the name, and
// returns the value that had previously been in memory.
#define ATOMIC_FETCH_AND_ADD(ptr,value) \
    __sync_fetch_and_add((ptr), (value))
#define ATOMIC_FETCH_AND_SUB(ptr,value) \
    __sync_fetch_and_sub((ptr), (value))
#define ATOMIC_FETCH_AND_OR(ptr,value) \
    __sync_fetch_and_or((ptr), (value))
#define ATOMIC_FETCH_AND_AND(ptr,value) \
    __sync_fetch_and_and((ptr), (value))
#define ATOMIC_FETCH_AND_XOR(ptr,value) \
    __sync_fetch_and_xor((ptr), (value))
#define ATOMIC_FETCH_AND_NAND(ptr,value) \
    __sync_fetch_and_nand((ptr), (value))

// These builtins perform the operation suggested by the name, and
// return the new value.
#define ATOMIC_ADD_AND_FETCH(ptr,value) \
    __sync_add_and_fetch((ptr), (value))
#define ATOMIC_SUB_AND_FETCH(ptr,value) \
    __sync_sub_and_fetch((ptr), (value))
#define ATOMIC_OR_AND_FETCH(ptr,value) \
    __sync_or_and_fetch((ptr), (value))
#define ATOMIC_AND_AND_FETCH(ptr,value) \
    __sync_and_and_fetch((ptr), (value))
#define ATOMIC_XOR_AND_FETCH(ptr,value) \
    __sync_xor_and_fetch((ptr), (value))
#define ATOMIC_NAND_AND_FETCH(ptr,value) \
    __sync_nand_and_fetch((ptr), (value))

// These builtins perform an atomic compare and swap. That is, if the
// current value of *ptr is oldval, then write newval into *ptr.
// The "bool" version returns true if the comparison is successful
// and newval was written. The "val" version returns the contents of
// *ptr before the operation.
#define ATOMIC_BOOL_COMPARE_AND_SWAP(ptr,oldval,newval) \
    __sync_bool_compare_and_swap((ptr), (oldval), (newval))
#define ATOMIC_VAL_COMPARE_AND_SWAP(ptr,oldval,newval) \
    __sync_val_compare_and_swap((ptr), (oldval), (newval))

// This builtin, as described by Intel, is not a traditional
// test-and-set operation, but rather an atomic exchange operation.
// It writes value into *ptr, and returns the previous contents of
// *ptr.
#define ATOMIC_LOCK_TEST_AND_SET(ptr,value) \
    __sync_lock_test_and_set((ptr), (value))

// This builtin issues a full memory barrier.
#define MEMORY_BARRIER() __sync_synchronize()

// Atomic flag operations
#define ATOMIC_FLAG_DECLARE(flag) \
    struct { unsigned long volatile val; } flag
#define ATOMIC_FLAG_SET_TRUE(flag) \
    __sync_nand_and_fetch(&(flag).val, 0)
#define ATOMIC_FLAG_SET_FALSE(flag) \
    __sync_and_and_fetch(&(flag).val, 0)
#define ATOMIC_FLAG_IS_TRUE(flag) \
    __sync_or_and_fetch(&(flag).val, 0)
#define ATOMIC_FLAG_IS_FALSE(flag) \
    __sync_or_and_fetch(&(flag).val, 0)

// Atomic variable operations
#define ATOMIC_VAR_DECLARE(type,var) \
    struct { type volatile val; } var
#define ATOMIC_VAR_WRITE(var,value) \
    __sync_synchronize(); \
    __sync_lock_test_and_set(&(var).val, value)
#define ATOMIC_VAR_READ(var) \
    __sync_or_and_fetch(&(var).val, 0)

#endif

