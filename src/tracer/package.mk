# Rules for the tracer package

protodefs += \
  tracer/log.proto

srcs += \
  tracer/loader.cc \
  tracer/loader_main.cc \
  tracer/log.cc \
  tracer/log.pb.cc \
  tracer/profiler.cpp \
  tracer/profiler_main.cpp \
  tracer/recorder.cc

pintools += \
  tracer_profiler.so

cmdtools += \
  tracer_loader

tracer_profiler_objs := \
  tracer/log.o \
  tracer/log.pb.o \
  tracer/profiler.o \
  tracer/profiler_main.o \
  tracer/recorder.o \
  $(core_objs)

tracer_loader_objs := \
  tracer/loader.o \
  tracer/loader_main.o \
  tracer/log.o \
  tracer/log.pb.o \
  $(core_cmd_objs)

tracer_objs := \
  tracer/loader.o \
  tracer/log.o \
  tracer/log.pb.o \
  tracer/profiler.o \
  tracer/recorder.o

tracer_cmd_objs := \
  tracer/loader.o \
  tracer/log.o \
  tracer/log.pb.o

