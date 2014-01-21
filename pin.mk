# Rules for the PIN binary instrumentation tool.

ifndef PIN_HOME
  $(error Please define PIN_HOME environment.)
endif

PIN_ROOT := $(PIN_HOME)

CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
include $(CONFIG_ROOT)/makefile.config
#include $(TOOLS_ROOT)/Config/makefile.default.rules

# Check if this is a PinPlay kit.
ifneq ($(wildcard $(PIN_HOME)/extras/pinplay),)
  $(info *** Building with PinPlay kit ***)

  PINPLAY_HOME := $(PIN_HOME)/extras/pinplay
  CFLAGS += -DCONFIG_PINPLAY
  CXXFLAGS += -DCONFIG_PINPLAY
  INCS += -I$(PINPLAY_HOME)/include
  INCS += -I$(PINPLAY_HOME)/include-ext
  TOOL_LPATHS += -L$(PINPLAY_HOME)/lib/$(TARGET)
  TOOL_LPATHS += -L$(PINPLAY_HOME)/lib-ext/$(TARGET)

  # PinPlay library must be loaded fist.
  TOOL_LIBS := -lpinplay -lbz2 -lz $(TOOL_LIBS)
endif
