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

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <cassert>

#include <map>

#include "core/basictypes.h"
#include "core/logging.h"
#include "core/pin_util.hpp"

// The base class for function wrappers.
class WrapperBase {
 public:
  virtual std::string name() = 0;
  virtual std::string func() = 0;
  virtual std::string lib() = 0;
  virtual std::string group() = 0;
  virtual AFUNPTR ori_funptr() = 0;
  virtual void set_ori_funptr(AFUNPTR ori_funptr) = 0;

  THREADID tid() { return tid_; }
  CONTEXT *ctxt() { return ctxt_; }
  ADDRINT ret_addr() { return ret_addr_; }

 protected:
  // Do not allow it to be instantiated directly.
  WrapperBase() {}
  virtual ~WrapperBase() {}

  // These variables store context information.
  THREADID tid_;
  CONTEXT *ctxt_;
  ADDRINT ret_addr_;

 private:
  friend class WrapperFactory;

  DISALLOW_COPY_CONSTRUCTORS(WrapperBase);
};

// The dirty macro tricks for defining wrapper classes. This will be replaced by
// C++0x variadic templates.

// Define argument iterators. We avoid to use the boost library.
#define MEMBER_ARGS_0
#define MEMBER_ARGS_1 A0 arg0_
#define MEMBER_ARGS_2 MEMBER_ARGS_1; A1 arg1_
#define MEMBER_ARGS_3 MEMBER_ARGS_2; A2 arg2_
#define MEMBER_ARGS_4 MEMBER_ARGS_3; A3 arg3_
#define MEMBER_ARGS(i) MEMBER_ARGS_##i

#define ARGS_0
#define ARGS_1 , A0 arg0
#define ARGS_2 ARGS_1, A1 arg1
#define ARGS_3 ARGS_2, A2 arg2
#define ARGS_4 ARGS_3, A3 arg3
#define ARGS(i) ARGS_##i

#define SET_ARGS_0
#define SET_ARGS_1 wrapper.arg0_ = arg0
#define SET_ARGS_2 SET_ARGS_1; wrapper.arg1_ = arg1
#define SET_ARGS_3 SET_ARGS_2; wrapper.arg2_ = arg2
#define SET_ARGS_4 SET_ARGS_3; wrapper.arg3_ = arg3
#define SET_ARGS(i) SET_ARGS_##i

#define PARGS_0
#define PARGS_1 PIN_PARG(A0), arg0_,
#define PARGS_2 PARGS_1 PIN_PARG(A1), arg1_,
#define PARGS_3 PARGS_2 PIN_PARG(A2), arg2_,
#define PARGS_4 PARGS_3 PIN_PARG(A3), arg3_,
#define PARGS(i) PARGS_##i

#define PROTO_PARGS_0
#define PROTO_PARGS_1 PIN_PARG(A0),
#define PROTO_PARGS_2 PROTO_PARGS_1 PIN_PARG(A1),
#define PROTO_PARGS_3 PROTO_PARGS_2 PIN_PARG(A2),
#define PROTO_PARGS_4 PROTO_PARGS_3 PIN_PARG(A3),
#define PROTO_PARGS(i) PROTO_PARGS_##i

#define IARGS_0
#define IARGS_1 IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
#define IARGS_2 IARGS_1 IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
#define IARGS_3 IARGS_2 IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
#define IARGS_4 IARGS_3 IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
#define IARGS(i) IARGS_##i

#define TYPENAME_ARGS_0
#define TYPENAME_ARGS_1 , typename A0
#define TYPENAME_ARGS_2 TYPENAME_ARGS_1, typename A1
#define TYPENAME_ARGS_3 TYPENAME_ARGS_2, typename A2
#define TYPENAME_ARGS_4 TYPENAME_ARGS_3, typename A3
#define TYPENAME_ARGS(i) TYPENAME_ARGS_##i

#define PARAM_ARGS_0
#define PARAM_ARGS_1 A0
#define PARAM_ARGS_2 PARAM_ARGS_1, A1
#define PARAM_ARGS_3 PARAM_ARGS_2, A2
#define PARAM_ARGS_4 PARAM_ARGS_3, A3
#define PARAM_ARGS(i) PARAM_ARGS_##i

#define ARG_ACCESSORS_0
#define ARG_ACCESSORS_1 A0 arg0() { return arg0_; }
#define ARG_ACCESSORS_2 ARG_ACCESSORS_1 A1 arg1() { return arg1_; }
#define ARG_ACCESSORS_3 ARG_ACCESSORS_2 A2 arg2() { return arg2_; }
#define ARG_ACCESSORS_4 ARG_ACCESSORS_3 A3 arg3() { return arg3_; }
#define ARG_ACCESSORS(i) ARG_ACCESSORS_##i

// Define member functions.
#define MEMBERS(R, NUM_ARGS)                                                \
  R ret_val_;                                                               \
  MEMBER_ARGS(NUM_ARGS)

#define MEMBERS_NORET(NUM_ARGS)                                             \
  MEMBER_ARGS(NUM_ARGS)

#define ACCESSORS(R, NUM_ARGS)                                              \
  R ret_val() { return ret_val_; }                                          \
  void set_ret_val(R ret_val) { ret_val_ = ret_val; }                       \
  ARG_ACCESSORS(NUM_ARGS)

#define ACCESSORS_NORET(NUM_ARGS)                                           \
  ARG_ACCESSORS(NUM_ARGS)

#define CALL_ORIGINAL(R, NUM_ARGS)                                          \
  void CallOriginal() {                                                     \
    assert(ori_funptr_);                                                    \
    PIN_CallApplicationFunction(                                            \
        ctxt_,                                                              \
        tid_,                                                               \
        CALLINGSTD_DEFAULT,                                                 \
        ori_funptr_,                                                        \
        PIN_PARG(R), &ret_val_,                                             \
        PARGS(NUM_ARGS)                                                     \
        PIN_PARG_END());                                                    \
  }

#define CALL_ORIGINAL_NORET(NUM_ARGS)                                       \
  void CallOriginal() {                                                     \
    assert(ori_funptr_);                                                    \
    PIN_CallApplicationFunction(                                            \
        ctxt_,                                                              \
        tid_,                                                               \
        CALLINGSTD_DEFAULT,                                                 \
        ori_funptr_,                                                        \
        PIN_PARG(void),                                                     \
        PARGS(NUM_ARGS)                                                     \
        PIN_PARG_END());                                                    \
  }

#define ACTIVATE(R, NUM_ARGS)                                               \
  void Activate(RTN rtn, HandlerType handler) {                             \
    assert(RTN_Valid(rtn));                                                 \
    handler_ = handler;                                                     \
    PROTO proto = PROTO_Allocate(                                           \
        PIN_PARG(R),                                                        \
        CALLINGSTD_DEFAULT,                                                 \
        func().c_str(),                                                     \
        PROTO_PARGS(NUM_ARGS)                                               \
        PIN_PARG_END());                                                    \
    RTN_ReplaceSignature(                                                   \
        rtn,                                                                \
        (AFUNPTR)ReplacedFunction,                                          \
        IARG_PROTOTYPE, proto,                                              \
        IARG_THREAD_ID,                                                     \
        IARG_CONST_CONTEXT,                                                 \
        IARG_RETURN_IP,                                                     \
        IARGS(NUM_ARGS)                                                     \
        IARG_END);                                                          \
  }

#define ACTIVATE_NORET(NUM_ARGS) ACTIVATE(void, NUM_ARGS)

#define REPLACED_FUNCTION(R, NUM_ARGS)                                      \
  static R ReplacedFunction(THREADID tid,                                   \
                            CONTEXT *ctxt,                                  \
                            ADDRINT ret_addr                                \
                            ARGS(NUM_ARGS)) {                               \
    W wrapper;                                                              \
    wrapper.tid_ = tid;                                                     \
    wrapper.ctxt_ = ctxt;                                                   \
    wrapper.ret_addr_ = ret_addr;                                           \
    SET_ARGS(NUM_ARGS);                                                     \
    handler_(&wrapper);                                                     \
    return wrapper.ret_val_;                                                \
  }

#define REPLACED_FUNCTION_NORET(NUM_ARGS)                                   \
  static void ReplacedFunction(THREADID tid,                                \
                               CONTEXT *ctxt,                               \
                               ADDRINT ret_addr                             \
                               ARGS(NUM_ARGS)) {                            \
    W wrapper;                                                              \
    wrapper.tid_ = tid;                                                     \
    wrapper.ctxt_ = ctxt;                                                   \
    wrapper.ret_addr_ = ret_addr;                                           \
    SET_ARGS(NUM_ARGS);                                                     \
    handler_(&wrapper);                                                     \
  }

// Define wrapper templates.
#define WRAPPER_TEMPLATE(NUM_ARGS)                                          \
  template <typename W,                                                     \
            typename R                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  class Wrapper<W, R(PARAM_ARGS(NUM_ARGS))> : public WrapperBase {          \
   public:                                                                  \
    typedef void (*HandlerType)(W *);                                       \
                                                                            \
    Wrapper() {}                                                            \
    virtual ~Wrapper() {}                                                   \
                                                                            \
    AFUNPTR ori_funptr() { return ori_funptr_; }                            \
    HandlerType handler() { return handler_; }                              \
    void set_ori_funptr(AFUNPTR ori_funptr) { ori_funptr_ = ori_funptr; }   \
    void set_handler(HandlerType handler) { handler_ = handler; }           \
                                                                            \
    CALL_ORIGINAL(R, NUM_ARGS);                                             \
    ACTIVATE(R, NUM_ARGS);                                                  \
    ACCESSORS(R, NUM_ARGS);                                                 \
                                                                            \
   protected:                                                               \
    REPLACED_FUNCTION(R, NUM_ARGS);                                         \
                                                                            \
    MEMBERS(R, NUM_ARGS);                                                   \
                                                                            \
    static AFUNPTR ori_funptr_;                                             \
    static HandlerType handler_;                                            \
                                                                            \
   private:                                                                 \
    friend class WrapperFactory;                                            \
                                                                            \
    DISALLOW_COPY_CONSTRUCTORS(Wrapper);                                    \
  };                                                                        \
                                                                            \
  template <typename W,                                                     \
            typename R                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  AFUNPTR Wrapper<W, R(PARAM_ARGS(NUM_ARGS))>::ori_funptr_ = NULL;          \
  template <typename W,                                                     \
            typename R                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  void (*Wrapper<W, R(PARAM_ARGS(NUM_ARGS))>::handler_)(W *) = NULL


#define WRAPPER_TEMPLATE_NORET(NUM_ARGS)                                    \
  template <typename W                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  class Wrapper<W, void(PARAM_ARGS(NUM_ARGS))> : public WrapperBase {       \
   public:                                                                  \
    typedef void (*HandlerType)(W *);                                       \
                                                                            \
    Wrapper() {}                                                            \
    virtual ~Wrapper() {}                                                   \
                                                                            \
    AFUNPTR ori_funptr() { return ori_funptr_; }                            \
    HandlerType handler() { return handler_; }                              \
    void set_ori_funptr(AFUNPTR ori_funptr) { ori_funptr_ = ori_funptr; }   \
    void set_handler(HandlerType handler) { handler_ = handler; }           \
                                                                            \
    CALL_ORIGINAL_NORET(NUM_ARGS);                                          \
    ACTIVATE_NORET(NUM_ARGS);                                               \
    ACCESSORS_NORET(NUM_ARGS);                                              \
                                                                            \
   protected:                                                               \
    REPLACED_FUNCTION_NORET(NUM_ARGS);                                      \
                                                                            \
    MEMBERS_NORET(NUM_ARGS);                                                \
                                                                            \
    static AFUNPTR ori_funptr_;                                             \
    static HandlerType handler_;                                            \
                                                                            \
   private:                                                                 \
    friend class WrapperFactory;                                            \
                                                                            \
    DISALLOW_COPY_CONSTRUCTORS(Wrapper);                                    \
  };                                                                        \
                                                                            \
  template <typename W                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  AFUNPTR Wrapper<W, void(PARAM_ARGS(NUM_ARGS))>::ori_funptr_ = NULL;       \
  template <typename W                                                      \
            TYPENAME_ARGS(NUM_ARGS)>                                        \
  void (*Wrapper<W, void(PARAM_ARGS(NUM_ARGS))>::handler_)(W *) = NULL


// The class for individual function wrappers.
template <typename W, typename Signature>
class Wrapper;

// Define wrapper classes.
WRAPPER_TEMPLATE(0);
WRAPPER_TEMPLATE(1);
WRAPPER_TEMPLATE(2);
WRAPPER_TEMPLATE(3);
WRAPPER_TEMPLATE(4);

WRAPPER_TEMPLATE_NORET(0);
WRAPPER_TEMPLATE_NORET(1);
WRAPPER_TEMPLATE_NORET(2);
WRAPPER_TEMPLATE_NORET(3);
WRAPPER_TEMPLATE_NORET(4);

// The factory class for function wrappers.
class WrapperFactory {
 public:
  template <typename W>
  static W *Register() {
    return GetInstance()->Register_<W>();
  }

  template <typename W>
  static W *Find(const std::string &name) {
    return GetInstance()->Find_<W>(name);
  }

 protected:
  typedef std::map<std::string, WrapperBase *> WrapperMap;

  // Do not allow it to be instantiated by others.
  WrapperFactory() {}
  ~WrapperFactory() {}

  template <typename W>
  W *Register_() {
    W *wrapper = new W;
    wrappers_[wrapper->name()] = wrapper;
    return wrapper;
  }

  template <typename W>
  W *Find_(const std::string &name) {
    WrapperMap::iterator it = wrappers_.find(name);
    if (it == wrappers_.end())
      return NULL;
    return dynamic_cast<W *>(it->second);
  }

  static WrapperFactory *GetInstance() {
    if (!instance_) {
      instance_ = new WrapperFactory;
    }
    return instance_;
  }

  // A map between wrapper name and wrapper instance.
  WrapperMap wrappers_;

  // The singleton factory instance.
  static WrapperFactory *instance_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(WrapperFactory);
};

#define WRAPPER_CLASS(name) Wrapper##name

#define WRAPPER(name_, func_, lib_, group_, sig_)                           \
  class WRAPPER_CLASS(name_): public Wrapper<WRAPPER_CLASS(name_), sig_> {  \
   public:                                                                  \
    ~WRAPPER_CLASS(name_)() {}                                              \
    WRAPPER_CLASS(name_)() {}                                               \
                                                                            \
    std::string name() { return #name_; }                                   \
    std::string func() { return func_; }                                    \
    std::string lib() { return lib_; }                                      \
    std::string group() { return group_; }                                  \
                                                                            \
    static WRAPPER_CLASS(name_) *instance;                                  \
                                                                            \
   private:                                                                 \
    friend class WrapperFactory;                                            \
                                                                            \
    DISALLOW_COPY_CONSTRUCTORS(WRAPPER_CLASS(name_));                       \
  }

#define REGISTER_WRAPPER(name)                                              \
  WRAPPER_CLASS(name) *WRAPPER_CLASS(name)::instance =                      \
    WrapperFactory::Register<WRAPPER_CLASS(name)>()

#define ACTIVATE_WRAPPER(name, img, handler)                                \
  do {                                                                      \
    WRAPPER_CLASS(name) *wrapper =                                          \
      WrapperFactory::Find<WRAPPER_CLASS(name)>(#name);                     \
    assert(wrapper);                                                        \
    if (IMG_Name(img).find(wrapper->lib()) != std::string::npos) {          \
      DEBUG_FMT_PRINT_SAFE("Activate wrapper %s in %s\n",                   \
                           wrapper->func().c_str(),                         \
                           wrapper->lib().c_str());                         \
      RTN rtn = FindRTN(img, wrapper->func());                              \
      assert(RTN_Valid(rtn));                                               \
      wrapper->set_ori_funptr(RTN_Funptr(rtn));                             \
      wrapper->Activate(rtn, handler);                                      \
    }                                                                       \
  } while (0)

// Declare wrappers.
WRAPPER(Malloc, "malloc", "libc.so", "malloc", void *(size_t));
WRAPPER(Calloc, "calloc", "libc.so", "malloc", void *(size_t, size_t));
WRAPPER(Realloc, "realloc", "libc.so", "malloc", void *(void *, size_t));
WRAPPER(Free, "free", "libc.so", "malloc", void(void *));
WRAPPER(Valloc, "valloc", "libc.so", "malloc", void *(size_t));

WRAPPER(Sleep, "sleep", "libc.so", "unistd", unsigned int(unsigned int));
WRAPPER(Usleep, "usleep", "libc.so", "unistd", int(useconds_t));

WRAPPER(SchedSetScheduler, "sched_setscheduler", "libc.so", "sched", int(pid_t, int, struct sched_param *));
WRAPPER(SchedYield, "sched_yield", "libc.so", "sched", int(void));
WRAPPER(SchedSetAffinity, "sched_setaffinity", "libc.so", "sched", int(pid_t, size_t, cpu_set_t *));
WRAPPER(SetPriority, "setpriority", "libc.so", "sched", int(int, int, int));

WRAPPER(PthreadCreate, "pthread_create", "libpthread.so", "pthread", int(pthread_t *, pthread_attr_t *, void *(*)(void *), void *));
WRAPPER(PthreadJoin, "pthread_join", "libpthread.so", "pthread", int(pthread_t, void **));
WRAPPER(PthreadMutexTryLock, "pthread_mutex_trylock", "libpthread.so", "pthread", int(pthread_mutex_t *));
WRAPPER(PthreadMutexLock, "pthread_mutex_lock", "libpthread.so", "pthread", int(pthread_mutex_t *));
WRAPPER(PthreadMutexUnlock, "pthread_mutex_unlock", "libpthread.so", "pthread", int(pthread_mutex_t *));
WRAPPER(PthreadCondSignal, "pthread_cond_signal", "libpthread.so", "pthread", int(pthread_cond_t *));
WRAPPER(PthreadCondBroadcast, "pthread_cond_broadcast", "libpthread.so", "pthread", int(pthread_cond_t *));
WRAPPER(PthreadCondWait, "pthread_cond_wait", "libpthread.so", "pthread", int(pthread_cond_t *, pthread_mutex_t *));
WRAPPER(PthreadCondTimedwait, "pthread_cond_timedwait", "libpthread.so", "pthread", int(pthread_cond_t *, pthread_mutex_t *, struct timespec *));
WRAPPER(PthreadBarrierInit, "pthread_barrier_init", "libpthread.so", "pthread", int(pthread_barrier_t *, pthread_barrierattr_t *, unsigned int));
WRAPPER(PthreadBarrierWait, "pthread_barrier_wait", "libpthread.so", "pthread", int(pthread_barrier_t *));

#endif // end of #ifndef CORE_WRAPPER_HPP_

