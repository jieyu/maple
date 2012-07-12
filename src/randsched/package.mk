# Rules for the randsched package

protodefs += \
  randsched/history.proto

srcs += \
  randsched/history.cc \
  randsched/history.pb.cc \
  randsched/scheduler.cpp \
  randsched/scheduler_main.cpp

pintools += \
  randsched_scheduler.so

randsched_scheduler_objs := \
  randsched/history.o \
  randsched/history.pb.o \
  randsched/scheduler.o \
  randsched/scheduler_main.o \
  $(core_objs)

randsched_objs := \
  randsched/history.o \
  randsched/history.pb.o \
  randsched/scheduler.o

