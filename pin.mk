# Rules for the PIN binary instrumentation tool.

ifndef PIN_ROOT
  $(error Please define PIN_ROOT environment.)
endif

PIN_KIT = $(PIN_ROOT)
KIT = 1

TARGET_COMPILER ?= gnu
ifdef OS
  ifeq (${OS}, Windows_NT)
    TARGET_COMPILER = ms
  endif
endif

ifeq ($(TARGET_COMPILER), gnu)
  CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)
  PIN = $(PIN_ROOT)/pin
endif

ifeq ($(TARGET_COMPILER), ms)
  DBG ?=
  PIN = $(PIN_ROOT)/pin.bat
endif

