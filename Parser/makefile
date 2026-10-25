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

OBJ = obj/Tools_Cpp0.o obj/Tools_LineMap.o obj/Tools_Macros.o obj/Tools_Pp.o obj/Tools_PpCtx.o
TGT = Tools_Cpp0.Test Tools_LineMap.Test Tools_PpCtx.Test Tools_PpInc.Test Tools_Pp.Test

Tools_o = ../lib/Tools.Test.a obj/Tools_Cpp0.o obj/Tools_Macros.o
Tools_h = Tools_Core.h Tools_Class.h Tools_Core_Debug.h Tools_Buffer.h

obj/Tools_Cpp0.o : $(Tools_h) Tools_Cpp0.h
obj/Tools_LineMap.o : $(Tools_h) Tools_LineMap.h
obj/Tools_Macros.o : $(Tools_h) Tools_Macros.h
obj/Tools_Pp.o : $(Tools_h) Tools_Pp.h Tools_PpCtx.h Tools_Macros.h
obj/Tools_PpCtx.o : $(Tools_h) Tools_PpCtx.h

obj/Tools_PpCtx.Test.o : $(Tools_h) Tools_PpCtx.h
obj/Tools_PpLineMap.Test.o : $(Tools_h) Tools_LineMap.h
obj/Tools_PpInc.Test.o : $(Tools_h) Tools_Cpp0.h
obj/Tools_Pp.Test.o : $(Tools_h) Tools_Cpp0.h Tools_Macros.h Tools_PpCtx.h Tools_Pp.h
obj/Tools_Cpp0.Test.o : $(Tools_h) Tools_Cpp0.h

Tools_Cpp0.Test :  obj/Tools_Cpp0.o obj/Tools_Cpp0.Test.o $(Tools_o)
	gcc -o Tools_Cpp0.Test obj/Tools_Cpp0.Test.o $(Tools_o)
Tools_LineMap.Test :  obj/Tools_LineMap.o obj/Tools_LineMap.Test.o $(Tools_o)
	gcc -o Tools_LineMap.Test obj/Tools_LineMap.Test.o obj/Tools_LineMap.o $(Tools_o)
Tools_PpCtx.Test :  obj/Tools_PpCtx.o obj/Tools_PpCtx.Test.o $(Tools_o)
	gcc -o Tools_PpCtx.Test obj/Tools_PpCtx.Test.o obj/Tools_PpCtx.o $(Tools_o)
Tools_PpInc.Test :  obj/Tools_PpInc.Test.o obj/Tools_Pp.o obj/Tools_PpCtx.o obj/Tools_LineMap.o $(Tools_o)
	gcc -o Tools_PpInc.Test obj/Tools_PpInc.Test.o obj/Tools_Pp.o obj/Tools_LineMap.o obj/Tools_PpCtx.o $(Tools_o)
Tools_Pp.Test :  obj/Tools_Pp.o obj/Tools_Pp.Test.o obj/Tools_PpCtx.o obj/Tools_LineMap.o $(Tools_o)
	gcc -o Tools_Pp.Test obj/Tools_Pp.Test.o obj/Tools_Pp.o obj/Tools_PpCtx.o obj/Tools_LineMap.o $(Tools_o)

.PHONY : all
all : \
$(TGT) $(OBJ)

.PHONY : clean
clean:
	rm obj/*.o; rm $(TGT)
