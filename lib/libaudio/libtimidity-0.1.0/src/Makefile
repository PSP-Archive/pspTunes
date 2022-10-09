CC = psp-g++
AR = psp-ar
PSPDIR = $(shell psp-config --psp-prefix)
INCLUDES = -I. -I$(PSPDIR)/include
#CXXFLAGS = -g -W -Wall -ansi -pedantic
#CXXFLAGS = -pg -g -W -Wall -ansi -pedantic
CXXFLAGS = -O2 -G0 -W -Wall -fsingle-precision-constant -fno-exceptions
CFLAGS = -O2 -G0 -fsingle-precision-constant
TARGET_LIB = ../../libtimidity.a
OBJS = \
	common.o \
	instrum.o \
	instrum_dls.o \
	mix.o \
	output.o \
	playmidi.o \
	readmidi.o \
	resample.o \
	stream.o \
	tables.o \
	timidity.o \

all: libtimidity.a

libtimidity.a: $(OBJS)
	$(AR) -r libtimidity.a *.o


LIBS = -lm -lc 

$(OBJS): %.o: %.c
	$(CC) -c $(CXXFLAGS) $(INCLUDES) $<
	
install:
	@mkdir -p $(PSPDIR)/include/libtimidity $(PSPDIR)/lib 
	@cp *.h $(PSPDIR)/include/libtimidity
	@cp ../../libtimidity.a $(PSPDIR)/lib
#        find boost -name *.hpp -exec cp -R {} $(PSPDIR)/include/{} \;
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
