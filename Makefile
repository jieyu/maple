PIN_ROOT := $(shell echo $$PIN_HOME)
CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
include $(CONFIG_ROOT)/makefile.config
include $(TOOLS_ROOT)/Config/makefile.default.rules


# Top level makefile for the project.

########## CHANGE ACCORDINGLY ###########

compiletype ?= debug
pinplay ?= 0
packages := core tracer sinst pct randsched race systematic idiom
user_flags := -D_USING_DEBUG_INFO
ifeq ($(pinplay), 1)
    user_flags += -DUSE_PINPLAY
endif

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
ifeq ($(compiletype), debug)
  CFLAGS += -Wall -Werror -g -D_DEBUG
  CXXFLAGS += -Wall -Werror -g -D_DEBUG
else
  CFLAGS += -O3 -fomit-frame-pointer
  CXXFLAGS += -O3 -fomit-frame-pointer
endif
CFLAGS += -fPIC -D_GNU_SOURCE $(user_flags)
CXXFLAGS += -fPIC -D_GNU_SOURCE $(user_flags)
INCS := -I$(srcdir) -I$(PROTOBUF_HOME)/include
ifeq ($(pinplay), 1)
    INCS += -I$(PIN_ROOT)/extras/pinplay/include
    INCS += -I$(PIN_ROOT)/extras/pinplay/include-ext
endif
LDFLAGS += 
LPATHS += -L$(PROTOBUF_HOME)/lib -Wl,-rpath,$(PROTOBUF_HOME)/lib
LIBS += -lprotobuf
PIN_LDFLAGS +=
PIN_LPATHS += -L$(PROTOBUF_HOME)/lib -Wl,-rpath,$(PROTOBUF_HOME)/lib
PIN_LIBS += -lrt -lprotobuf

# gen dependency
cxxgendepend = $(CXX) $(CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<
pincxxgendepend = $(CXX) $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(PIN_CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<

# rules
.SECONDEXPANSION:

all: $(pintools) $(cmdtools) proto-scripts
	
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

ifeq ($(pinplay), 1)
    PINPLAY_LIB_HOME=$(PIN_ROOT)/extras/pinplay/lib/$(TARGET)
    PINPLAY_EXT_LIB_HOME=$(PIN_ROOT)/extras/pinplay/lib-ext/$(TARGET)
endif

$(pincxxobjs): $(builddir)%.o : $(srcdir)%.cpp
	@$(pincxxgendepend);
	$(CXX) -c $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(PIN_CXXFLAGS) $(INCS) -o $@ $<

ifeq ($(pinplay), 1)
$(pintools): $(builddir)%.so : $$(%_objs) $(PINPLAY_LIB_HOME)/libpinplay.a $(PINPLAY_EXT_LIB_HOME)/libbz2.a $(PINPLAY_EXT_LIB_HOME)/libz.a
	$(LINKER) $(TOOL_LDFLAGS) $(PIN_LDFLAGS) $(LINK_DEBUG) ${LINK_EXE}$@ $^ $(TOOL_LPATHS) $(TOOL_LIBS) ${PIN_LPATHS} $(PIN_LIBS) $(DBG)
else
$(pintools): $(builddir)%.so : $$(%_objs)
	$(LINKER) $(TOOL_LDFLAGS) $(PIN_LDFLAGS) $(LINK_DEBUG) ${LINK_EXE}$@ $^ $(TOOL_LPATHS) $(TOOL_LIBS) ${PIN_LPATHS} $(PIN_LIBS) $(DBG)
endif

$(cmdtools): $(builddir)% : $$(%_objs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LPATHS) $(LIBS)

clean:
	rm -rf build-debug build-release
	rm -f $(protosrcs) $(protohdrs)
	rm -rf $(protoscriptdir)

# include dependencies
-include $(cxxsrcs:$(srcdir)%.cc=$(builddir)%.d)
-include $(pincxxsrcs:$(srcdir)%.cpp=$(builddir)%.d)

