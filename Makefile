
ifeq ($(mtk), y)
DIR=./mtk
CROSS-PREFIX=/opt/buildroot-gcc463/usr/bin/mipsel-linux-
else ifeq ($(qtn), y)
DIR=./qtn
CROSS-PREFIX=/usr/local/ARC/gcc//bin/arc-linux-uclibc-
else
DIR=./x86
CROSS-PREFIX=
endif
#CROSS-PREFIX=/opt/buildroot-gcc463/usr/bin/mipsel-linux-
#CROSS-PREFIX=

CC=$(CROSS-PREFIX)gcc
STRIP=$(CROSS-PREFIX)strip

OBJS=etest.o
#SRCS:=$(OBJS:.o=.c)
SRCS:=$(subst .o,.c,$(OBJS))


ifeq ($(block),y)
CFLAGS += -DBLOCK_IN_MAIN
endif
ifeq ($(usepselect),y)
CFLAGS += -DUSE_PSELECT
endif

LDFLAGS += -lc -lgcc -lm -lpthread

#etest_thread: $(OBJS)
%: 
	$(CC) $(CFLAGS) -c -o $(OBJS) $(SRCS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	$(STRIP)   $@
	chmod +x $@
	mkdir -p $(DIR)
	cp -rf $@ $(DIR)

.PHONY: clean

clean:
	find ./ -name "*.o" -print | xargs rm -f
	rm -f et etb
