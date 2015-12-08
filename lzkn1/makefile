CC =     gcc
WARNINGS = -Wall
CFLAGS = $(WARNINGS) -D ADD_EXPORTS -Os -c -Iobj -I.
LFLAGS = $(WARNINGS)

all: clean obj dll

clean:
	rm -rf obj
	rm -f LZKN1.dll
	rm -f resource.o
	
obj:
	mkdir obj
	
dll: obj/main.o resinfo
	$(CC) $(LFLAGS) obj/main.o resource.o -shared -o LZKN1.dll

resinfo:
	windres -i resource.rc -o resource.o
	
obj/main.o: main.c main.h
	$(CC) $(CFLAGS) main.c -o obj/main.o