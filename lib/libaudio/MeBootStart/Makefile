TARGET = MeBootStart
OBJS = main.o sceMeCore_driver3xx.o

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c -x assembler-with-cpp

BUILD_PRX = 1

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

PSP_FW_VERSION = 371

LIBDIR = 
LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspkernel


PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
