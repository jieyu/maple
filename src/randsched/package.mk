# Rules for the randsched package

protodefs += randsched/history.proto

srcs += randsched/scheduler_main.cpp \
        randsched/scheduler.cpp \
        randsched/history.cc \
        randsched/history.pb.cc

pintools += randsched_scheduler.so

randsched_scheduler_objs := randsched/scheduler_main.o \
                            randsched/scheduler.o \
                            randsched/history.o \
                            randsched/history.pb.o \
                            $(core_objs)

randsched_objs := randsched/scheduler.o \
                  randsched/history.o \
                  randsched/history.pb.o

