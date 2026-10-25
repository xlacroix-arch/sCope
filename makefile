CC = gcc
#CFLAGS = -g -O2 -Iinc
OS_ARCH = A_Linux
CFLAGS = -g -Iinc -Itest/inc -Isrc/inc
LDFLAGS = -lpthread -lrt
vpath  %.c src:src/A_linux:test
vpath  %.s src:src/A_linux
vpath  %.h inc:test/inc:src/inc
vpath  %.o .:obj

obj/%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

Tools_o = obj/Tools_Core.o obj/Tools_List.o obj/Tools_Stack.o obj/Tools_Fiber.o obj/$(OS_ARCH)/fiber.o obj/Tools_MemPool.o obj/Tools_ChunkArena.o obj/Tools_MainPool.o obj/Tools_Gate.o obj/Tools_MemPoolFilters.o obj/Tools_MemPoolMini.o obj/Tools_MemPoolPush.o obj/Tools_Buffer.o obj/Tools_Dict.o obj/Tools_Expr.o obj/Tools_OutBuf.o obj/Tools_Tok.o
Tools_h = Tools_Core.h Tools_Class.h Tools_Core_Debug.h

obj/Tools_Buffer.o : $(Tools_h) Tools_Buffer.h
obj/Tools_Dict.o : $(Tools_h) Tools_Dict.h
obj/Tools_Expr.o : $(Tools_h) Tools_Expr.h Tools_Tok.h
obj/Tools_Tok.o : $(Tools_h) Tools_Tok.h
obj/Tools_OutBuf.o : $(Tools_h) Tools_OutBuf.h
obj/Tools_Core.o : $(Tools_h)
obj/Tools_Stack.o : $(Tools_h)
obj/Tools_List.o : $(Tools_h)
obj/Tools_Fiber.o : $(Tools_h)
obj/Tools_MemPool.o : $(Tools_h)
obj/Tools_MemPoolMini.o : $(Tools_h)
obj/Tools_MemPoolPush.o : $(Tools_h)
obj/Tools_Gate.o : $(Tools_h) Tools_Gate.h
obj/Tools_ChunkArena.o : $(Tools_h) Tools_Chunk.h
obj/Tools_MainPool.o : $(Tools_h) Tools_Chunk.h
obj/Tools_Main.o : $(Tools_h) Tools_Main.h Tools_Gate.h
obj/Tools_MemPoolFilters.o : $(Tools_h)

obj/Tools_Fiber_test.o : $(Tools_h)

obj/Tools_Buffer.Test.o : $(Tools_h) Tools_Buffer.h
obj/Tools_Main.Test.o : $(Tools_h) Tools_Gate.h Tools_Main.h
obj/Tools_MainPool.Test.o : $(Tools_h)
obj/Tools_Gate.Test.o : $(Tools_h) Tools_Gate.h
obj/Tools_Gate.Test.Thread.o : $(Tools_h) Tools_Gate.h MallocPool.h
obj/Tools_ChunkArena.Test.o : $(Tools_h) Tools_Chunk.h
obj/Tools_Core.Test.o : $(Tools_h)
obj/Tools_List.Test.o : $(Tools_h)
obj/Tools_MemPoolFilters.o : $(Tools_h)

obj/$(OS_ARCH)/fiber.o : $(Tools_h) fiber.h
obj/fiber_test.o : fiber.h $(Tools_h)

lib/Tools.Test.a : $(Tools_o)
	gcc -o lib/Tools.Test.a -r $(Tools_o)
lib/Tools.a : $(Tools_o) obj/Tools_Main.o
	gcc -o lib/Tools.a -r $(Tools_o) obj/Tools_Main.o
Tools_OutBuf.Test : Tools_Core.h obj/Tools_OutBuf.Test.o lib/Tools.Test.a
	gcc -o Tools_OutBuf.Test obj/Tools_OutBuf.Test.o lib/Tools.Test.a
Tools_Tok.Test : Tools_Core.h obj/Tools_Tok.Test.o lib/Tools.Test.a
	gcc -o Tools_Tok.Test obj/Tools_Tok.Test.o lib/Tools.Test.a
Tools_Expr.Test : Tools_Core.h obj/Tools_Expr.Test.o lib/Tools.Test.a
	gcc -o Tools_Expr.Test obj/Tools_Expr.Test.o lib/Tools.Test.a
Tools_Main.Test : Tools_Core.h obj/Tools_Main.Test.o lib/Tools.a
	gcc -o Tools_Main.Test obj/Tools_Main.Test.o lib/Tools.a
Tools_Buffer.Test : Tools_Core.h obj/Tools_Buffer.Test.o lib/Tools.Test.a
	gcc -o Tools_Buffer.Test obj/Tools_Buffer.Test.o lib/Tools.Test.a
Tools_Gate.Thread.Test : Tools_Core.h obj/Tools_Gate.Thread.Test.o lib/Tools.Test.a
	gcc -o Tools_Gate.Thread.Test obj/Tools_Gate.Thread.Test.o lib/Tools.Test.a
Tools_Gate.Test : Tools_Core.h obj/Tools_Gate.Test.o lib/Tools.Test.a
	gcc -o Tools_Gate.Test obj/Tools_Gate.Test.o lib/Tools.Test.a
Tools_MemPoolFilters.Test : Tools_Core.h obj/Tools_MemPoolFilters.Test.o lib/Tools.Test.a
	gcc -o Tools_MemPoolFilters.Test obj/Tools_MemPoolFilters.Test.o lib/Tools.Test.a
Tools_MemPoolMini.Test : Tools_Core.h obj/Tools_MemPoolMini.Test.o lib/Tools.Test.a
	gcc -o Tools_MemPoolMini.Test obj/Tools_MemPoolMini.Test.o lib/Tools.Test.a
Tools_MemPoolPush.Test : Tools_Core.h obj/Tools_MemPoolPush.Test.o lib/Tools.Test.a
	gcc -o Tools_MemPoolPush.Test obj/Tools_MemPoolPush.Test.o lib/Tools.Test.a
Tools_Core.Test : Tools_Core.h obj/Tools_Core.Test.o lib/Tools.Test.a
	gcc -o Tools_Core.Test obj/Tools_Core.Test.o lib/Tools.Test.a
Tools_List.Test : Tools_Core.h obj/Tools_List.Test.o lib/Tools.Test.a
	gcc -o Tools_List.Test obj/Tools_List.Test.o lib/Tools.Test.a
Tools_Fiber_test : Tools_Core.h obj/Tools_Fiber_test.o lib/Tools.Test.a
	gcc -o Tools_Fiber_test obj/Tools_Fiber_test.o lib/Tools.Test.a
fiber_test : obj/fiber_test.o $(Tools_o)
	gcc -o fiber_test obj/fiber_test.o obj/$(OS_ARCH)/fiber.o

TGT= Tools_Core.Test Tools_List.Test fiber_test Tools_MemPoolFilters.Test Tools_Gate.Thread.Test Tools_Main.Test Tools_MemPoolMini.Test Tools_MemPoolPush.Test Tools_Buffer.Test Tools_Expr.Test Tools_OutBuf.Test Tools_Tok.Test lib/Tools.a lib/Tools.Test.a

.PHONY : all
all : $(Tools_o) $(TGT)

.PHONY : clean
clean:
	rm obj/*.o; rm obj/$(OS_ARCH)/*.o; rm $(TGT)
