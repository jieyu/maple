# Rules for the race package

protodefs += race/race.proto

srcs += race/profiler_main.cpp \
        race/profiler.cpp \
        race/pct_profiler_main.cpp \
        race/pct_profiler.cpp \
        race/djit.cc \
        race/fasttrack.cc \
        race/detector.cc \
        race/race.cc \
        race/race.pb.cc

pintools += race_profiler.so \
            race_pct_profiler.so

race_profiler_objs := race/profiler_main.o \
                      race/profiler.o \
                      race/djit.o \
                      race/fasttrack.o \
                      race/detector.o \
                      race/race.o \
                      race/race.pb.o \
                      $(core_objs)

race_pct_profiler_objs := race/pct_profiler_main.o \
                          race/pct_profiler.o \
                          race/djit.o \
                          race/fasttrack.o \
                          race/detector.o \
                          race/race.o \
                          race/race.pb.o \
                          $(pct_objs) \
                          $(core_objs)

race_objs := race/djit.o \
             race/fasttrack.o \
             race/detector.o \
             race/race.o \
             race/race.pb.o

