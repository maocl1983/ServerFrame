DEBUG	= -g -DDEBUG
OPT	= -funroll-loops -D__USE_STRING_INLINES -fhosted

CC	= gcc

SRCS 	= time.c timer.c

INCL	+= -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/
CFLAGS	= -Wall $(INCL) -MMD $(DEBUG) $(OPT) -fPIC

all: $(SRCS:.c=.o)
	@-cp *.o ../
install:
	@-mkdir /usr/include/libcommon/time
	-cp -v *.h /usr/include/libcommon/time

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<
	@-mv -f $*.d .dep.$@

clean:
	@-rm -f *.o *.po *.so *.d .dep.* $(BINS) core

-include /dev/null $(wildcard .dep.*)
