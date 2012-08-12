# Rules for the idiom package

protodefs += \
  idiom/history.proto \
  idiom/iroot.proto \
  idiom/memo.proto

srcs += \
  idiom/chess_profiler.cpp \
  idiom/chess_profiler_main.cpp \
  idiom/history.cc \
  idiom/history.pb.cc \
  idiom/iroot.cc \
  idiom/iroot.pb.cc \
  idiom/memo.cc \
  idiom/memo.pb.cc \
  idiom/memo_tool.cc \
  idiom/memo_tool_main.cc \
  idiom/observer.cc \
  idiom/observer_new.cc \
  idiom/pct_profiler.cpp \
  idiom/pct_profiler_main.cpp \
  idiom/predictor.cc \
  idiom/predictor_new.cc \
  idiom/profiler.cpp \
  idiom/profiler_main.cpp \
  idiom/randsched_profiler.cpp \
  idiom/randsched_profiler_main.cpp \
  idiom/scheduler.cpp \
  idiom/scheduler_common.cpp \
  idiom/scheduler_main.cpp

pintools += \
  idiom_chess_profiler.so \
  idiom_pct_profiler.so \
  idiom_profiler.so \
  idiom_randsched_profiler.so \
  idiom_scheduler.so

cmdtools += \
  idiom_memo_tool

iroot_objs += \
  idiom/history.o \
  idiom/history.pb.o \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/scheduler_common.o

idiom_scheduler_objs := \
  idiom/history.o \
  idiom/history.pb.o \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/observer.o \
  idiom/observer_new.o \
  idiom/scheduler.o \
  idiom/scheduler_common.o \
  idiom/scheduler_main.o \
  $(sinst_objs) \
  $(core_objs)

idiom_profiler_objs := \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/observer.o \
  idiom/observer_new.o \
  idiom/predictor.o \
  idiom/predictor_new.o \
  idiom/profiler.o \
  idiom/profiler_main.o \
  $(sinst_objs) \
  $(core_objs)

idiom_pct_profiler_objs := \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/observer.o \
  idiom/observer_new.o \
  idiom/pct_profiler.o \
  idiom/pct_profiler_main.o \
  idiom/predictor.o \
  idiom/predictor_new.o \
  $(sinst_objs) \
  $(pct_objs) \
  $(core_objs)

idiom_randsched_profiler_objs := \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/observer.o \
  idiom/observer_new.o \
  idiom/predictor.o \
  idiom/predictor_new.o \
  idiom/randsched_profiler.o \
  idiom/randsched_profiler_main.o \
  $(sinst_objs) \
  $(randsched_objs) \
  $(core_objs)

idiom_chess_profiler_objs := \
  idiom/chess_profiler.o \
  idiom/chess_profiler_main.o \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/observer.o \
  idiom/observer_new.o \
  $(sinst_objs) \
  $(race_objs) \
  $(systematic_objs) \
  $(core_objs)

idiom_memo_tool_objs := \
  idiom/iroot.o \
  idiom/iroot.pb.o \
  idiom/memo.o \
  idiom/memo.pb.o \
  idiom/memo_tool.o \
  idiom/memo_tool_main.o \
  $(core_cmd_objs)

