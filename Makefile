SHELL = /bin/sh

srcdir= .

CC = g++ -O3 -pedantic -Wall -Wno-long-long -Wno-variadic-macros


LIBS = boost_program_options-mt boost_regex rt

CDEBUG = -g
CFLAGS = $(CDEBUG) -I. -I$(srcdir) $(DEFS) \
        -DDEF_AR_FILE=\"$(DEF_AR_FILE)\" \
        -DDEFBLOCKING=$(DEFBLOCKING)
LDFLAGS = -g

SRCS = testharness.cpp abstraction/vcookiestore.cpp


.PHONY: all
all: nop_testharness mem_testharness cb_testharness

.PHONY: clean
clean:
	rm -f nop_testharness mem_testharness
	
mem_testharness:	$(SRCS)
	$(CC) -DSTORAGE_ENGINE=VCStoreInMemory -L/usr/lib64/boost141 $(LIBS:%=-l%) -o $@ $(SRCS)

nop_testharness:	$(SRCS)
	$(CC) -DSTORAGE_ENGINE=VCStoreNOP -L/usr/lib64/boost141 $(LIBS:%=-l%) -o $@ $(SRCS)
