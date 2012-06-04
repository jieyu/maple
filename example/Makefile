# Toplevel makefile for examples

subdirs := $(wildcard */)
subdirs_clean := $(subdirs:%/=%_clean)

.PHONY: all clean $(subdirs) $(subdirs_clean)

all: $(subdirs)

$(subdirs):
	$(MAKE) -C $@

clean: $(subdirs_clean)

$(subdirs_clean):
	$(MAKE) -C $(patsubst %_clean,%,$@) clean

