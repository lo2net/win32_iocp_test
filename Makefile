all : server.exe client.exe server-2.exe

server-2.exe : server-2.o
	link.exe -debug -out:$@ $<
	mt.exe -manifest $@.manifest -outputresource:'$@;1'

server.exe : server.o
	link.exe -debug -out:$@ $<
	mt.exe -manifest $@.manifest -outputresource:'$@;1'

client.exe : client.o
	link.exe -debug -out:$@ $<
	mt.exe -manifest $@.manifest -outputresource:'$@;1'

%.o : %.cpp
	cl.exe -c -MD -Zi -EHsc -DWIN32 -DWIN64 -DWINDOWS -D_WIN64 $< -Fo$@

clean :
	rm -rf *.o *.ilk *.pdb *.manifest
	rm -rf server.exe client.exe server-2.exe
