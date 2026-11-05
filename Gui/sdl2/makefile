CC = gcc
#CFLAGS = -g -O2 -Iinc
CFLAGS = -g -Iinc -I../inc -I../../inc -I/usr/include/SDL2
LDFLAGS = -lpthread -lrt $(shell sdl2-config --libs)
vpath  %.c src:src/A_linux
vpath  %.s src
vpath  %.a lib:../lib:../../lib
vpath  %.h inc:../inc:../../inc
vpath  %.o .:obj:../obj:../../obj

Core_h = Tools_Core.h Tools_Class.h
TGT = SdlFirst.Test Sdl2nd.Test Rhombic.Test GlyphEdit

obj/%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

obj/SdlFirst.o : $(Core_h)
obj/TlsGui.o : $(Core_h) TlsGuiCtrl.h TlsGui.Local.h TlsGui.Ext.h
obj/SdlGui.o : $(Core_h) TlsGuiCtrl.h
obj/GlyphEdit.o : $(Core_h) TlsGuiCtrl.h
obj/SdlFirst.Test.o : $(Core_h) TlsGuiCtrl.h
obj/Sdl2nd.Test.o : $(Core_h) TlsGuiCtrl.h
obj/Rhombic.Test.o : $(Core_h) TlsGuiCtrl.h TlsGui.Local.h
ojb/TlsGuiMesh.o : $(Core_h) TlsGuiCtrl.h TlsGui.Local.h
SdlFirst.Test : obj/SdlFirst.Test.o Tools.a obj/TlsGui.o obj/SdlGui.o obj/TlsGui.Local.o
	$(CC)  $^ $(LDFLAGS) -o $@
Sdl2nd.Test :  obj/Sdl2nd.Test.o Tools.a obj/TlsGui.o obj/SdlGui.o obj/TlsGui.Local.o obj/GuiFont8x8.o
	$(CC)  $^ $(LDFLAGS) -o $@
Rhombic.Test :  obj/Rhombic.Test.o Tools.a obj/TlsGui.o obj/SdlGui.o obj/TlsGui.Local.o obj/GuiFont8x8.o obj/TlsGuiMesh.o
	$(CC)  $^ $(LDFLAGS) -lm -o $@
TlsGui.Local.o : $(Core_h) TlsGui.Local.h TlsGuiCtrl.h
GlyphEdit :  obj/GlyphEdit.o Tools.a obj/TlsGui.o obj/SdlGui.o obj/TlsGui.Local.o obj/GuiFont8x8.o
	$(CC)  $^ $(LDFLAGS) -o $@

.PHONY : all
all : $(TGT) obj/TlsGui.o obj/SdlGui.o obj/TlsGui.Local.o

.PHONY : clean
clean:
	rm obj/*.o; rm $(TGT)
