# Top level makefile for the project.

########## CHANGE ACCORDINGLY ###########

compiletype ?= debug
packages := core tracer sinst pct randsched race systematic idiom
user_flags := -D_USING_DEBUG_INFO

########## DO NOT CHANGE BELOW ##########

# define source and build dir
ifeq ($(compiletype), debug)
  DEBUG := 1
endif

# include Protobuf environment
include protobuf.mk

# include PIN environment
include pin.mk

# define source, script, build dir
srcdir := src/
scriptdir := script/
protoscriptdir := $(scriptdir)maple/proto/

ifeq ($(compiletype), debug)
  builddir := build-debug/
else ifeq ($(compiletype), release)
  builddir := build-release/
else
  $(error Please specify compile type correctly, debug or release.)
endif

# process rules for each package
$(foreach pkg,$(packages),$(eval include $(srcdir)$(pkg)/package.mk))
$(foreach pkg,$(packages),$(eval objdirs += $(builddir)$(pkg)/))
protosrcs := $(protodefs:%.proto=$(srcdir)%.pb.cc)
protohdrs := $(protodefs:%.proto=$(srcdir)%.pb.h)
protoscripts := $(protodefs:%.proto=$(protoscriptdir)%_pb2.py)
protopkgscripts := $(sort $(patsubst %,$(protoscriptdir)%__init__.py,$(dir $(protodefs))) $(protoscriptdir)__init__.py)
cxxsrcs := $(filter %.cc,$(srcs:%=$(srcdir)%))
pincxxsrcs := $(filter %.cpp,$(srcs:%=$(srcdir)%))
cxxobjs := $(cxxsrcs:$(srcdir)%.cc=$(builddir)%.o)
pincxxobjs := $(pincxxsrcs:$(srcdir)%.cpp=$(builddir)%.o)
pintool_names := $(basename $(pintools))
cmdtool_names := $(basename $(cmdtools))
pintools := $(pintools:%=$(builddir)%)
cmdtools := $(cmdtools:%=$(builddir)%)
$(foreach name,$(pintool_names),$(eval $(name)_objs := $($(name)_objs:%=$(builddir)%)))
$(foreach name,$(cmdtool_names),$(eval $(name)_objs := $($(name)_objs:%=$(builddir)%)))

# set compile flags
CFLAGS += -fPIC -D_GNU_SOURCE $(user_flags)
CXXFLAGS += -fPIC -D_GNU_SOURCE $(user_flags)
INCS += -I$(srcdir) -I$(PROTOBUF_HOME)/include
LDFLAGS += 
LPATHS += -L$(PROTOBUF_HOME)/lib -Wl,-rpath,$(PROTOBUF_HOME)/lib
LIBS += -lprotobuf
TOOL_LDFLAGS +=
TOOL_LPATHS += -L$(PROTOBUF_HOME)/lib -Wl,-rpath,$(PROTOBUF_HOME)/lib
TOOL_LIBS += -lrt -lprotobuf

ifeq ($(compiletype), debug)
  CFLAGS += -Wall -Werror -g -D_DEBUG
  CXXFLAGS += -Wall -Werror -g -D_DEBUG
else
  CFLAGS += -O3 -fomit-frame-pointer
  CXXFLAGS += -O3 -fomit-frame-pointer
endif

# gen dependency
cxxgendepend = $(CXX) $(CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<
pincxxgendepend = $(CXX) $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<

# set the default goal to be maple-all
.DEFAULT_GOAL := maple-all

# rules
.SECONDEXPANSION:

maple-all: $(pintools) $(cmdtools) proto-scripts
	
proto-scripts: $(protoscripts) $(protopkgscripts)

$(cxxobjs) : | $(objdirs)

$(pincxxobjs) : | $(objdirs)

$(protoscripts) : | $(protoscriptdir)

$(cxxobjs) : $(protosrcs) $(protohdrs)

$(pincxxobjs) : $(protosrcs) $(protohdrs)

$(protopkgscripts) : $(protoscripts)

$(objdirs):
	mkdir -p $@

$(protoscriptdir):
	mkdir -p $@

$(protosrcs): $(srcdir)%.pb.cc : $(srcdir)%.proto
	$(PROTOC) -I=$(srcdir) --cpp_out=$(srcdir) $<

$(protohdrs): $(srcdir)%.pb.h : $(srcdir)%.proto
	$(PROTOC) -I=$(srcdir) --cpp_out=$(srcdir) $<

$(protoscripts): $(protoscriptdir)%_pb2.py : $(srcdir)%.proto
	$(PROTOC) -I=$(srcdir) --python_out=$(protoscriptdir) $<

$(protopkgscripts):
	touch $@

$(cxxobjs): $(builddir)%.o : $(srcdir)%.cc
	@$(cxxgendepend);
	$(CXX) -c $(CXXFLAGS) $(INCS) -o $@ $<

$(pincxxobjs): $(builddir)%.o : $(srcdir)%.cpp
	@$(pincxxgendepend);
	$(CXX) $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(INCS) $(COMP_OBJ)$@ $<

$(pintools): $(builddir)%.so : $$(%_objs) $(CONTROLLER_LIB)
	$(LINKER) $(TOOL_LDFLAGS) $(LINK_DEBUG) ${LINK_EXE}$@ $^ ${TOOL_LPATHS} $(TOOL_LIBS) $(CONTROLLER_LIB) $(DBG)

$(cmdtools): $(builddir)% : $$(%_objs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LPATHS) $(LIBS)

clean:
	rm -rf build-debug build-release
	rm -f $(protosrcs) $(protohdrs)
	rm -rf $(protoscriptdir)

# include dependencies
-include $(cxxsrcs:$(srcdir)%.cc=$(builddir)%.d)
-include $(pincxxsrcs:$(srcdir)%.cpp=$(builddir)%.d)

