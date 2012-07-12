# Rules for the sinst package

protodefs += \
  sinst/sinst.proto

srcs += \
  sinst/analyzer.cc \
  sinst/profiler.cpp \
  sinst/profiler_main.cpp \
  sinst/sinst.cc \
  sinst/sinst.pb.cc

pintools += \
  sinst_profiler.so

sinst_profiler_objs := \
  sinst/analyzer.o \
  sinst/profiler.o \
  sinst/profiler_main.o \
  sinst/sinst.o \
  sinst/sinst.pb.o \
  $(core_objs)

sinst_objs := \
  sinst/analyzer.o \
  sinst/profiler.o \
  sinst/sinst.o \
  sinst/sinst.pb.o

sinst_cmd_objs := \
  sinst/analyzer.o \
  sinst/sinst.o \
  sinst/sinst.pb.o

