##########################################################################################
# 	Assignment 4
#  
#	Author: Camille Gandotra
#  	CruzID: cgandotr
#
#  	Makefile
#  	~ Makefile for ASGN 4
#
#########################################################################################

EXECBIN  = httpserver
SOURCES  = $(wildcard *.c)
OBJECTS  = $(SOURCES:%.c=%.o)
FORMATS  = $(SOURCES:%.c=%.fmt)

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wpedantic -Werror -Wextra

.PHONY: all clean format

all: $(EXECBIN)

$(EXECBIN): $(OBJECTS)
	$(CC) -o $@ $^ asgn4_helper_funcs.a

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXECBIN) $(OBJECTS)

format: $(FORMATS)

%.fmt: %.c
	$(FORMAT) -i $<
	touch $@

