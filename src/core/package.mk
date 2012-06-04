# Rules for core package

protodefs += core/static_info.proto

srcs += core/static_info.pb.cc \
        core/logging.cc \
        core/stat.cc \
        core/static_info.cc \
        core/descriptor.cc \
        core/debug_analyzer.cc \
        core/filter.cc \
        core/vector_clock.cc \
        core/lock_set.cc \
        core/cmdline_knob.cc \
        core/offline_tool.cc \
        core/pin_util.cpp \
        core/pthread_wrapper.cpp \
        core/malloc_wrapper.cpp \
        core/sched_wrapper.cpp \
        core/unistd_wrapper.cpp \
        core/pin_knob.cpp \
        core/execution_control.cpp

core_objs := core/static_info.pb.o \
             core/logging.o \
             core/stat.o \
             core/static_info.o \
             core/descriptor.o \
             core/debug_analyzer.o \
             core/filter.o \
             core/vector_clock.o \
             core/lock_set.o \
             core/cmdline_knob.o \
             core/offline_tool.o \
             core/pin_util.o \
             core/pthread_wrapper.o \
             core/malloc_wrapper.o \
             core/sched_wrapper.o \
             core/unistd_wrapper.o \
             core/pin_knob.o \
             core/execution_control.o

core_cmd_objs := core/static_info.pb.o \
                 core/logging.o \
                 core/stat.o \
                 core/static_info.o \
                 core/descriptor.o \
                 core/debug_analyzer.o \
                 core/filter.o \
                 core/vector_clock.o \
                 core/lock_set.o \
                 core/cmdline_knob.o \
                 core/offline_tool.o

