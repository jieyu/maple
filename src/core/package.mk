# Rules for core package

protodefs += \
  core/static_info.proto

srcs += \
  core/callstack.cc \
  core/cmdline_knob.cc \
  core/debug_analyzer.cc \
  core/descriptor.cc \
  core/execution_control.cpp \
  core/filter.cc \
  core/knob.cc \
  core/lock_set.cc \
  core/logging.cc \
  core/offline_tool.cc \
  core/pin_knob.cpp \
  core/pin_util.cpp \
  core/stat.cc \
  core/static_info.cc \
  core/static_info.pb.cc \
  core/vector_clock.cc \
  core/wrapper.cpp

core_objs := \
  core/callstack.o \
  core/cmdline_knob.o \
  core/debug_analyzer.o \
  core/descriptor.o \
  core/execution_control.o \
  core/filter.o \
  core/knob.o \
  core/lock_set.o \
  core/logging.o \
  core/offline_tool.o \
  core/pin_knob.o \
  core/pin_util.o \
  core/stat.o \
  core/static_info.o \
  core/static_info.pb.o \
  core/vector_clock.o \
  core/wrapper.o

core_cmd_objs := \
  core/callstack.o \
  core/cmdline_knob.o \
  core/debug_analyzer.o \
  core/descriptor.o \
  core/filter.o \
  core/knob.o \
  core/lock_set.o \
  core/logging.o \
  core/offline_tool.o \
  core/stat.o \
  core/static_info.o \
  core/static_info.pb.o \
  core/vector_clock.o \

