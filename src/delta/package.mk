# Rules for the delta package
#

protodefs += delta/ilist.proto \
             delta/slist.proto

srcs += delta/profiler_main.cpp \
        delta/randsched_profiler_main.cpp \
        delta/scheduler_avoid_main.cpp \
        delta/scheduler_keep_main.cpp \
        delta/profiler.cpp \
        delta/randsched_profiler.cpp \
        delta/scheduler_avoid.cpp \
        delta/scheduler_keep.cpp \
        delta/observer.cc \
        delta/ilist.cc \
        delta/ilist.pb.cc \
        delta/slist.cc \
        delta/slist.pb.cc

pintools += delta_profiler.so \
            delta_randsched_profiler.so \
            delta_scheduler_avoid.so \
            delta_scheduler_keep.so

ilist_objs += delta/ilist.o \
              delta/ilist.pb.o

slist_objs += delta/slist.o \
              delta/slist.pb.o

delta_profiler_objs := delta/profiler_main.o \
                       delta/profiler.o \
                       delta/observer.o \
                       delta/ilist.o \
                       delta/ilist.pb.o \
                       $(iroot_objs) \
                       $(core_objs)

delta_randsched_profiler_objs := delta/randsched_profiler_main.o \
                                delta/randsched_profiler.o \
                                delta/observer.o \
                                delta/ilist.o \
                                delta/ilist.pb.o \
                                $(iroot_objs) \
                                $(randsched_objs) \
                                $(core_objs)

delta_scheduler_avoid_objs := delta/scheduler_avoid_main.o \
                              delta/scheduler_avoid.o \
                              delta/observer.o \
                              delta/ilist.o \
                              delta/ilist.pb.o \
                              delta/slist.o \
                              delta/slist.pb.o \
                              $(iroot_objs) \
                              $(core_objs)
                        
delta_scheduler_keep_objs := delta/scheduler_keep_main.o \
                             delta/scheduler_keep.o \
                             delta/observer.o \
                             delta/ilist.o \
                             delta/ilist.pb.o \
                             delta/slist.o \
                             delta/slist.pb.o \
                             $(iroot_objs) \
                             $(core_objs)
