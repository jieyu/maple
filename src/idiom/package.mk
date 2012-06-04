# Rules for the idiom package

protodefs += idiom/iroot.proto \
             idiom/memo.proto \
             idiom/history.proto

srcs += idiom/scheduler_main.cpp \
        idiom/profiler_main.cpp \
        idiom/pct_profiler_main.cpp \
        idiom/randsched_profiler_main.cpp \
        idiom/chess_profiler_main.cpp \
        idiom/scheduler.cpp \
        idiom/scheduler_common.cpp \
        idiom/profiler.cpp \
        idiom/pct_profiler.cpp \
        idiom/randsched_profiler.cpp \
        idiom/chess_profiler.cpp \
        idiom/predictor.cc \
        idiom/predictor_new.cc \
        idiom/observer.cc \
        idiom/observer_new.cc \
        idiom/memo.cc \
        idiom/memo.pb.cc \
        idiom/iroot.cc \
        idiom/iroot.pb.cc \
        idiom/history.cc \
        idiom/history.pb.cc

pintools += idiom_scheduler.so \
            idiom_profiler.so \
            idiom_pct_profiler.so \
            idiom_randsched_profiler.so \
            idiom_chess_profiler.so \

iroot_objs += idiom/scheduler_common.o \
              idiom/history.o \
              idiom/history.pb.o \
              idiom/iroot.o \
              idiom/iroot.pb.o

idiom_scheduler_objs := idiom/scheduler_main.o \
                        idiom/scheduler.o \
                        idiom/scheduler_common.o \
                        idiom/observer.o \
                        idiom/observer_new.o \
                        idiom/memo.o \
                        idiom/memo.pb.o \
                        idiom/iroot.o \
                        idiom/iroot.pb.o \
                        idiom/history.o \
                        idiom/history.pb.o \
                        $(sinst_objs) \
                        $(core_objs)

idiom_profiler_objs := idiom/profiler_main.o \
                       idiom/profiler.o \
                       idiom/predictor.o \
                       idiom/predictor_new.o \
                       idiom/observer.o \
                       idiom/observer_new.o \
                       idiom/memo.o \
                       idiom/memo.pb.o \
                       idiom/iroot.o \
                       idiom/iroot.pb.o \
                       $(sinst_objs) \
                       $(core_objs)

idiom_pct_profiler_objs := idiom/pct_profiler_main.o \
                           idiom/pct_profiler.o \
                           idiom/predictor.o \
                           idiom/predictor_new.o \
                           idiom/observer.o \
                           idiom/observer_new.o \
                           idiom/memo.o \
                           idiom/memo.pb.o \
                           idiom/iroot.o \
                           idiom/iroot.pb.o \
                           $(sinst_objs) \
                           $(pct_objs) \
                           $(core_objs)

idiom_randsched_profiler_objs := idiom/randsched_profiler_main.o \
                                 idiom/randsched_profiler.o \
                                 idiom/predictor.o \
                                 idiom/predictor_new.o \
                                 idiom/observer.o \
                                 idiom/observer_new.o \
                                 idiom/memo.o \
                                 idiom/memo.pb.o \
                                 idiom/iroot.o \
                                 idiom/iroot.pb.o \
                                 $(sinst_objs) \
                                 $(randsched_objs) \
                                 $(core_objs)

idiom_chess_profiler_objs := idiom/chess_profiler_main.o \
                             idiom/chess_profiler.o \
                             idiom/observer.o \
                             idiom/observer_new.o \
                             idiom/memo.o \
                             idiom/memo.pb.o \
                             idiom/iroot.o \
                             idiom/iroot.pb.o \
                             $(sinst_objs) \
                             $(race_objs) \
                             $(systematic_objs) \
                             $(core_objs)

