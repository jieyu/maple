# Rules for the race package

protodefs += \
  race/race.proto

srcs += \
  race/detector.cc \
  race/djit.cc \
  race/fasttrack.cc \
  race/pct_profiler.cpp \
  race/pct_profiler_main.cpp \
  race/profiler.cpp \
  race/profiler_main.cpp \
  race/race.cc \
  race/race.pb.cc

pintools += \
  race_pct_profiler.so \
  race_profiler.so

race_profiler_objs := \
  race/detector.o \
  race/djit.o \
  race/fasttrack.o \
  race/profiler.o \
  race/profiler_main.o \
  race/race.o \
  race/race.pb.o \
  $(core_objs)

race_pct_profiler_objs := \
  race/detector.o \
  race/djit.o \
  race/fasttrack.o \
  race/pct_profiler.o \
  race/pct_profiler_main.o \
  race/race.o \
  race/race.pb.o \
  $(pct_objs) \
  $(core_objs)

race_objs := \
  race/detector.o \
  race/djit.o \
  race/fasttrack.o \
  race/race.o \
  race/race.pb.o

