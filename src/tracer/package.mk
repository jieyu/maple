# Rules for the tracer package

protodefs += tracer/log.proto

srcs += tracer/profiler_main.cpp \
        tracer/loader_main.cc \
        tracer/profiler.cpp \
        tracer/loader.cc \
        tracer/recorder.cc \
        tracer/log.cc \
        tracer/log.pb.cc

pintools += tracer_profiler.so
cmdtools += tracer_loader

tracer_profiler_objs := tracer/profiler_main.o \
                        tracer/profiler.o \
                        tracer/recorder.o \
                        tracer/log.o \
                        tracer/log.pb.o \
                        $(core_objs)

tracer_loader_objs := tracer/loader_main.o \
                      tracer/loader.o \
                      tracer/log.o \
                      tracer/log.pb.o \
                      $(core_cmd_objs)

tracer_objs := tracer/profiler.o \
               tracer/loader.o \
               tracer/recorder.o \
               tracer/log.o \
               tracer/log.pb.o

tracer_cmd_objs := tracer/loader.o \
                   tracer/log.o \
                   tracer/log.pb.o

