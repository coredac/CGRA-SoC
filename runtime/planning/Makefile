ROOT := ../..
CC ?= cc
OUTPUT ?= $(ROOT)/build/native-query.so
GEMMINI := $(ROOT)/chipyard/generators/gemmini/software/gemmini-rocc-tests

.PHONY: all
all: $(OUTPUT)

$(OUTPUT): query.c workspace.h $(ROOT)/amoeba/include/Backend/SoC/Query.h $(GEMMINI)/include/gemmini_params.h
	mkdir -p $(dir $@)
	$(CC) -std=gnu11 -O2 -Wall -Wextra -Werror -fPIC -shared -I$(ROOT)/amoeba/include -I$(GEMMINI)/include query.c -o $@
