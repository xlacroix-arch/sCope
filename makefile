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

Tools_o = obj/Tools_Core.o obj/Tools_List.o obj/Tools_Stack.o obj/Tools_Fiber.o obj/$(OS_ARCH)/fiber.o obj/Tools_MemPool.o obj/Tools_ChunkArena.o obj/Tools_MainPool.o obj/Tools_Gate.o obj/Tools_MemPoolFilters.o obj/Tools_MemPoolMini.o obj/Tools_MemPoolPush.o obj/Tools_Buffer.o
Tools_h = Tools_Core.h Tools_Class.h Tools_Core_Debug.h

obj/Tools_Buffer.o : $(Tools_h) Tools_Buffer.h
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


Tools_Main.Test : Tools_Core.h obj/Tools_Main.Test.o $(Tools_o) obj/Tools_Main.o
	gcc -o Tools_Main.Test obj/Tools_Main.Test.o $(Tools_o) obj/Tools_Main.o
Tools_Buffer.Test : Tools_Core.h obj/Tools_Buffer.Test.o $(Tools_o) obj/Tools_Main.o
	gcc -o Tools_Buffer.Test obj/Tools_Buffer.Test.o $(Tools_o)
Tools_Gate.Thread.Test : Tools_Core.h obj/Tools_Gate.Thread.Test.o $(Tools_o)
	gcc -o Tools_Gate.Thread.Test obj/Tools_Gate.Thread.Test.o $(Tools_o)
Tools_Gate.Test : Tools_Core.h obj/Tools_Gate.Test.o $(Tools_o)
	gcc -o Tools_Gate.Test obj/Tools_Gate.Test.o $(Tools_o)
Tools_MemPoolFilters.Test : Tools_Core.h obj/Tools_MemPoolFilters.Test.o $(Tools_o)
	gcc -o Tools_MemPoolFilters.Test obj/Tools_MemPoolFilters.Test.o $(Tools_o)
Tools_MemPoolMini.Test : Tools_Core.h obj/Tools_MemPoolMini.Test.o $(Tools_o)
	gcc -o Tools_MemPoolMini.Test obj/Tools_MemPoolMini.Test.o $(Tools_o)
Tools_MemPoolPush.Test : Tools_Core.h obj/Tools_MemPoolPush.Test.o $(Tools_o)
	gcc -o Tools_MemPoolPush.Test obj/Tools_MemPoolPush.Test.o $(Tools_o)
Tools_Core.Test : Tools_Core.h obj/Tools_Core.Test.o $(Tools_o)
	gcc -o Tools_Core.Test obj/Tools_Core.Test.o $(Tools_o)
Tools_List.Test : Tools_Core.h obj/Tools_List.Test.o $(Tools_o)
	gcc -o Tools_List.Test obj/Tools_List.Test.o $(Tools_o)
Tools_Fiber_test : Tools_Core.h obj/Tools_Fiber_test.o $(Tools_o)
	gcc -o Tools_Fiber_test obj/Tools_Fiber_test.o $(Tools_o)
fiber_test : obj/fiber_test.o $(Tools_o)
	gcc -o fiber_test obj/fiber_test.o obj/$(OS_ARCH)/fiber.o

.PHONY : all
all : $(Tools_o) \
Tools_Core.Test Tools_List.Test fiber_test Tools_MemPoolFilters.Test Tools_Gate.Thread.Test Tools_Main.Test Tools_MemPoolMini.Test Tools_MemPoolPush.Test Tools_Buffer.Test

.PHONY : clean
clean:
	rm obj/*.o; rm obj/$(OS_ARCH)/*.o
