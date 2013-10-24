SHELL = /bin/sh

srcdir= .

CC = g++ -O3 -pedantic -Wall -Wno-long-long -Wno-variadic-macros -pthread -o term

LIBS = boost_program_options-mt boost_program_options boost_regex rt couchbase

CDEBUG = -g
CFLAGS = $(CDEBUG) -I. -I$(srcdir) $(DEFS) \
        -DDEF_AR_FILE=\"$(DEF_AR_FILE)\" \
        -DDEFBLOCKING=$(DEFBLOCKING)
LDFLAGS = -g

SRCS = testharness.cpp abstraction/vcookiestore.cpp VCCouchbaseStore.cc


.PHONY: all
all: nop_testharness mem_testharness cb_testharness

.PHONY: clean
clean:
	rm -f nop_testharness mem_testharness cb_testharness

mem_testharness:	$(SRCS)
	$(CC) -DSTORAGE_ENGINE=VCStoreInMemory -L/usr/lib $(LIBS:%=-l%) -o $@ $(SRCS)

nop_testharness:	$(SRCS)
	$(CC) -DSTORAGE_ENGINE=VCStoreNOP -L/usr/lib $(LIBS:%=-l%) -o $@ $(SRCS)

cb_testharness: 	$(SRCS)
	$(CC) -DSTORAGE_ENGINE=VCCouchbaseStore -L/usr/lib -L/usr/local/lib $(LIBS:%=-l%) -o $@ $(SRCS)
