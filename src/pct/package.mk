# Rules for the pct package

protodefs += pct/history.proto

srcs += pct/scheduler_main.cpp \
        pct/scheduler.cpp \
        pct/history.cc \
        pct/history.pb.cc

pintools += pct_scheduler.so

pct_scheduler_objs := pct/scheduler_main.o \
                      pct/scheduler.o \
                      pct/history.o \
                      pct/history.pb.o \
                      $(core_objs)

pct_objs := pct/scheduler.o \
            pct/history.o \
            pct/history.pb.o

