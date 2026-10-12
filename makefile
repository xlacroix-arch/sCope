CC = gcc
#CFLAGS = -g -O2 -Iinc
CFLAGS = -g -Iinc -Itest/inc
LDFLAGS = -lpthread -lrt
vpath  %.c src:src/A_linux:test
vpath  %.s src
vpath  %.h inc:test/inc
vpath  %.o .:obj

obj/%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

Tools_o = obj/Tools_Core.o obj/Tools_List.o obj/Tools_Stack.o obj/Tools_Fiber.o obj/fiber.o obj/Tools_MemPool.o
Tools_h = Tools_Core.h Tools_Class.h

obj/Tools_Core.o : $(Tools_h)
obj/Tools_Stack.o : $(Tools_h)
obj/Tools_List.o : $(Tools_h)
obj/Tools_Fiber.o : $(Tools_h)
obj/Tools_MemPool.o : $(Tools_h)

obj/Tools_Fiber_test.o : $(Tools_h)

obj/Tools_Core.Test.o : $(Tools_h)
obj/Tools_List.Test.o : $(Tools_h)

obj/fiber.o : $(Tools_h) fiber.h
obj/fiber_test.o : fiber.h $(Tools_h)


Tools_Core.Test : Tools_Core.h obj/Tools_Core.Test.o $(Tools_o)
	gcc -o Tools_Core.Test obj/Tools_Core.Test.o $(Tools_o)
Tools_List.Test : Tools_Core.h obj/Tools_List.Test.o $(Tools_o)
	gcc -o Tools_List.Test obj/Tools_List.Test.o $(Tools_o)
Tools_Fiber_test : Tools_Core.h obj/Tools_Fiber_test.o $(Tools_o)
	gcc -o Tools_Fiber_test obj/Tools_Fiber_test.o $(Tools_o)
fiber_test : obj/fiber_test.o $(Tools_o)
	gcc -o fiber_test obj/fiber_test.o obj/fiber.o

.PHONY : all
all : $(Tools_o) \
Tools_Core.Test Tools_List.Test fiber_test Tools_Fiber_test

.PHONY : clean
clean:
	rm obj/*.o
