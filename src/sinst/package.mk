# Rules for the sinst package

protodefs += sinst/sinst.proto

srcs += sinst/profiler_main.cpp \
        sinst/profiler.cpp \
        sinst/analyzer.cc \
        sinst/sinst.cc \
		sinst/sinst.pb.cc

pintools += sinst_profiler.so

sinst_profiler_objs := sinst/profiler_main.o \
                       sinst/profiler.o \
                       sinst/analyzer.o \
                       sinst/sinst.o \
					   sinst/sinst.pb.o \
                       $(core_objs)

sinst_objs := sinst/profiler.o \
              sinst/analyzer.o \
              sinst/sinst.o \
			  sinst/sinst.pb.o

sinst_cmd_objs := sinst/analyzer.o \
                  sinst/sinst.o \
				  sinst/sinst.pb.o

