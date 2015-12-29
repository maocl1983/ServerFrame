CC = g++

CFLAGS  = -Wall -pipe -DDEBUG -g -D_GNU_SOURCE -shared -fPIC -D_REENTRANT
LIB 	= -ldl -lrt  
INCLUDE =   -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/ -I../../

OO	= test.o 
TARGETS = test.so

all: $(OO) $(TARGETS)
	rm  *.o
	
$(TARGETS): $(OO)
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB)
#	rm ../bin/$(TARGETS); mv $(TARGETS) ../bin/
.cpp.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<
	echo $@
.cc.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<
	echo $@

%:%.cpp %.cc 
	$(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS)
	echo $@

clean:
	rm -f *.o
	rm -f $(TARGETS)

