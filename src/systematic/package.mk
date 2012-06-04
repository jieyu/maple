# Rules for the systematic package

protodefs += systematic/program.proto \
             systematic/search.proto \
             systematic/chess.proto

srcs += systematic/controller_main.cpp \
        systematic/controller.cpp \
        systematic/scheduler.cc \
        systematic/random.cc \
        systematic/chess.cc \
        systematic/chess.pb.cc \
        systematic/fair.cc \
        systematic/search.cc \
        systematic/search.pb.cc \
        systematic/program.cc \
        systematic/program.pb.cc

pintools += systematic_controller.so

systematic_controller_objs := systematic/controller_main.o \
                              systematic/controller.o \
                              systematic/scheduler.o \
                              systematic/random.o \
                              systematic/chess.o \
                              systematic/chess.pb.o \
                              systematic/fair.o \
                              systematic/search.o \
                              systematic/search.pb.o \
                              systematic/program.o \
                              systematic/program.pb.o \
                              $(race_objs) \
                              $(core_objs)

systematic_objs := systematic/controller.o \
                   systematic/scheduler.o \
                   systematic/random.o \
                   systematic/chess.o \
                   systematic/chess.pb.o \
                   systematic/fair.o \
                   systematic/search.o \
                   systematic/search.pb.o \
                   systematic/program.o \
                   systematic/program.pb.o \

