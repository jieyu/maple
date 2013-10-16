# Rules for the PIN binary instrumentation tool.

ifndef PIN_HOME
  $(error Please define PIN_HOME environment.)
endif

PIN_KIT = $(PIN_HOME)
KIT = 1

TARGET_COMPILER ?= gnu
ifdef OS
  ifeq (${OS}, Windows_NT)
    TARGET_COMPILER = ms
  endif
endif

ifeq ($(TARGET_COMPILER), gnu)
  CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)
  PIN = $(PIN_HOME)/pin
endif

ifeq ($(TARGET_COMPILER), ms)
  DBG ?=
  PIN = $(PIN_HOME)/pin.bat
endif

