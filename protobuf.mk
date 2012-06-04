# Rules for the Google Protobuf tool.

ifndef PROTOBUF_HOME
  $(error Please define PROTOBUF_HOME environment.)
endif

PROTOC=$(PROTOBUF_HOME)/bin/protoc

