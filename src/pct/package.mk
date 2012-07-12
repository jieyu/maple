# Rules for the pct package

protodefs += \
  pct/history.proto

srcs += \
  pct/history.cc \
  pct/history.pb.cc \
  pct/scheduler.cpp \
  pct/scheduler_main.cpp

pintools += \
  pct_scheduler.so

pct_scheduler_objs := \
  pct/history.o \
  pct/history.pb.o \
  pct/scheduler.o \
  pct/scheduler_main.o \
  $(core_objs)

pct_objs := \
  pct/history.o \
  pct/history.pb.o \
  pct/scheduler.o

