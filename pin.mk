# Rules for the PIN binary instrumentation tool.

ifndef PIN_HOME
  $(error Please define PIN_HOME environment.)
endif

PIN_ROOT := $(PIN_HOME)

CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
include $(CONFIG_ROOT)/makefile.config
#include $(TOOLS_ROOT)/Config/makefile.default.rules
