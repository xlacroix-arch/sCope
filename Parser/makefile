CC = gcc
#CFLAGS = -g -O2 -Iinc
OS_ARCH = A_Linux
CFLAGS = -g -I../inc -Iinc -Itest/inc -Isrc/inc
LDFLAGS = -lpthread -lrt
vpath  %.c src:src/A_linux:test
vpath  %.s src:src/A_linux
vpath  %.h ../inc:inc:test/inc:src/inc
vpath  %.o .:obj

obj/%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

Tools_o = ../lib/Tools.a
Tools_h = Tools_Core.h Tools_Class.h Tools_Core_Debug.h Tools_Buffer.h

obj/Tools_Cpp0.o : $(Tools_h) Tools_Cpp0.h

obj/Tools_Cpp0.Test.o : $(Tools_h) Tools_Cpp0.h

Tools_Cpp0.Test :  obj/Tools_Cpp0.o obj/Tools_Cpp0.Test.o $(Tools_o)
	gcc -o Tools_Cpp0.Test obj/Tools_Cpp0.Test.o obj/Tools_Cpp0.o $(Tools_o)

.PHONY : all
all : \
Tools_Cpp0.Test

.PHONY : clean
clean:
	rm obj/*.o; rm obj/$(OS_ARCH)/*.o
