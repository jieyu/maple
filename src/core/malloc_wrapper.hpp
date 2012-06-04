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

// File: core/malloc_wrapper.hpp - Define memory allocation function wrappers.
//
// Here are the prototypes of all the malloc functions on my machine
// (x86_64 RHEL5 with glibc-2.5): (* means implemented)
//
// * void *malloc (size_t __size) __THROW __attribute_malloc__ __wur;
// * void *calloc (size_t __nmemb, size_t __size) __THROW __attribute_malloc__ __wur;
// * void *realloc (void *__ptr, size_t __size) __THROW __attribute_malloc__ __attribute_warn_unused_result__;
// * void free (void *__ptr) __THROW;
// * void *valloc (size_t __size) __THROW __attribute_malloc__ __wur;
//   int posix_memalign (void **__memptr, size_t __alignment, size_t __size) __THROW __nonnull ((1)) __wur;
#ifndef CORE_MALLOC_WRAPPER_HPP_
#define CORE_MALLOC_WRAPPER_HPP_

#include "pin.H"

#include "core/basictypes.h"
#include "core/wrapper.hpp"

// Wrapper context for "malloc". The original function prototype is:
//
// void *malloc (size_t size);
//
class MallocContext : public WrapperContext {
 public:
  MallocContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr, size_t size)
      : WrapperContext(tid, ctxt, ret_addr),
        size_(size) {}
  ~MallocContext() {}

  void *ret_val() { return ret_val_; }
  size_t size() { return size_; }
  void set_ret_val(void *ret_val) { ret_val_ = ret_val; }

 private:
  void *ret_val_;
  size_t size_;

  DISALLOW_COPY_CONSTRUCTORS(MallocContext);
};

// Wrapper for "malloc".
class MallocWrapper
    : public Wrapper<MallocContext, MallocWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(MallocContext *context);

 private:
  MallocWrapper() {}
  ~MallocWrapper() {}

  static void *__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         size_t size);

  DISALLOW_COPY_CONSTRUCTORS(MallocWrapper);
};

// Wrapper context for "calloc". The original function prototype is:
//
// void *calloc (size_t nmemb, size_t size);
//
class CallocContext : public WrapperContext {
 public:
  CallocContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr, size_t nmemb,
                size_t size)
      : WrapperContext(tid, ctxt, ret_addr),
        nmemb_(nmemb),
        size_(size) {}
  ~CallocContext() {}

  void *ret_val() { return ret_val_; }
  size_t nmemb() { return nmemb_; }
  size_t size() { return size_; }
  void set_ret_val(void *ret_val) { ret_val_ = ret_val; }

 private:
  void *ret_val_;
  size_t nmemb_;
  size_t size_;

  DISALLOW_COPY_CONSTRUCTORS(CallocContext);
};

// Wrapper for "calloc".
class CallocWrapper
    : public Wrapper<CallocContext, CallocWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(CallocContext *context);

 private:
  CallocWrapper() {}
  ~CallocWrapper() {}

  static void *__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         size_t nmemb, size_t size);

  DISALLOW_COPY_CONSTRUCTORS(CallocWrapper);
};

// Wrapper context for "realloc". The original function prototype is:
//
// void *realloc (void *ptr, size_t size);
//
class ReallocContext : public WrapperContext {
 public:
  ReallocContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr, void *ptr,
                 size_t size)
      : WrapperContext(tid, ctxt, ret_addr),
        ptr_(ptr),
        size_(size) {}
  ~ReallocContext() {}

  void *ret_val() { return ret_val_; }
  void *ptr() { return ptr_; }
  size_t size() { return size_; }
  void set_ret_val(void *ret_val) { ret_val_ = ret_val; }

 private:
  void *ret_val_;
  void *ptr_;
  size_t size_;

  DISALLOW_COPY_CONSTRUCTORS(ReallocContext);
};

// Wrapper for "realloc".
class ReallocWrapper
    : public Wrapper<ReallocContext, ReallocWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(ReallocContext *context);

 private:
  ReallocWrapper() {}
  ~ReallocWrapper() {}

  static void *__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         void *ptr, size_t size);

  DISALLOW_COPY_CONSTRUCTORS(ReallocWrapper);
};

// Wrapper context for "free". The original function prototype is:
//
// void free (void *ptr);
//
class FreeContext : public WrapperContext {
 public:
  FreeContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr, void *ptr)
      : WrapperContext(tid, ctxt, ret_addr),
        ptr_(ptr) {}
  ~FreeContext() {}

  void *ptr() { return ptr_; }

 private:
  void *ptr_;

  DISALLOW_COPY_CONSTRUCTORS(FreeContext);
};

// Wrapper for "free".
class FreeWrapper
    : public Wrapper<FreeContext, FreeWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(FreeContext *context);

 private:
  FreeWrapper() {}
  ~FreeWrapper() {}

  static void __Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                        void *ptr);

  DISALLOW_COPY_CONSTRUCTORS(FreeWrapper);
};

// Wrapper context for "valloc". The original function prototype is:
//
// void *valloc (size_t size);
//
class VallocContext : public WrapperContext {
 public:
  VallocContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr, size_t size)
      : WrapperContext(tid, ctxt, ret_addr),
        size_(size) {}
  ~VallocContext() {}

  void *ret_val() { return ret_val_; }
  size_t size() { return size_; }
  void set_ret_val(void *ret_val) { ret_val_ = ret_val; }

 private:
  void *ret_val_;
  size_t size_;

  DISALLOW_COPY_CONSTRUCTORS(VallocContext);
};

// Wrapper for "valloc".
class VallocWrapper
    : public Wrapper<VallocContext, VallocWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(VallocContext *context);

 private:
  VallocWrapper() {}
  ~VallocWrapper() {}

  static void *__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         size_t size);

  DISALLOW_COPY_CONSTRUCTORS(VallocWrapper);
};

// Wrapper context for "memalign". The original function prototype is:
//
// void *memalign (size_t boundary, size_t size);
//
class MemalignContext : public WrapperContext {
 public:
  MemalignContext(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                  size_t boundary, size_t size)
      : WrapperContext(tid, ctxt, ret_addr),
        boundary_(boundary),
        size_(size) {}
  ~MemalignContext() {}

  void *ret_val() { return ret_val_; }
  size_t boundary() { return boundary_; }
  size_t size() { return size_; }
  void set_ret_val(void *ret_val) { ret_val_ = ret_val; }

 private:
  void * ret_val_;
  size_t boundary_;
  size_t size_;

  DISALLOW_COPY_CONSTRUCTORS(MemalignContext);
};

// Wrapper for "memalign".
class MemalignWrapper
    : public Wrapper<MemalignContext, MemalignWrapper> {
 public:
  static void Replace(IMG img, HandlerType handler);
  static void CallOriginal(MemalignContext *context);

 private:
  MemalignWrapper() {}
  ~MemalignWrapper() {}

  static void *__Wrapper(THREADID tid, CONTEXT *ctxt, ADDRINT ret_addr,
                         size_t boundary, size_t size);

  DISALLOW_COPY_CONSTRUCTORS(MemalignWrapper);
};

// Global functions to install malloc wrappers
void register_malloc_wrappers(IMG img);

#endif

