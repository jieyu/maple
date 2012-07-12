# Rules for the systematic package

protodefs += \
  systematic/chess.proto \
  systematic/program.proto \
  systematic/search.proto

srcs += \
  systematic/chess.cc \
  systematic/chess.pb.cc \
  systematic/controller.cpp \
  systematic/controller_main.cpp \
  systematic/fair.cc \
  systematic/program.cc \
  systematic/program.pb.cc \
  systematic/random.cc \
  systematic/scheduler.cc \
  systematic/search.cc \
  systematic/search.pb.cc

pintools += \
  systematic_controller.so

systematic_controller_objs := \
  systematic/chess.o \
  systematic/chess.pb.o \
  systematic/controller.o \
  systematic/controller_main.o \
  systematic/fair.o \
  systematic/program.o \
  systematic/program.pb.o \
  systematic/random.o \
  systematic/scheduler.o \
  systematic/search.o \
  systematic/search.pb.o \
  $(race_objs) \
  $(core_objs)

systematic_objs := \
  systematic/chess.o \
  systematic/chess.pb.o \
  systematic/controller.o \
  systematic/fair.o \
  systematic/program.o \
  systematic/program.pb.o \
  systematic/random.o \
  systematic/scheduler.o \
  systematic/search.o \
  systematic/search.pb.o

