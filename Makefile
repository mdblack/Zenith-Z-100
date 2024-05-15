z100 : mainboard.o 8085.o 8088.o e8259.o e8253.o jwd1797.o keyboard.o video.o screen.o debug.o
	gcc -pthread -o z100 mainboard.o 8085.o 8088.o e8259.o e8253.o jwd1797.o keyboard.o video.o screen.o debug.o `pkg-config --cflags --libs gtk+-3.0` -export-dynamic
mainboard.o : mainboard.c mainboard.h 8085.h 8088.h e8259.h e8253.h jwd1797.h keyboard.h video.h screen.h
	gcc -c mainboard.c `pkg-config --cflags --libs gtk+-3.0` -export-dynamic
8085.o : 8085.c 8085.h mainboard.h
	gcc -c 8085.c
8088.o : 8088.c 8088.h mainboard.h
	gcc -c 8088.c
e8259.o : e8259.c e8259.h
	gcc -c e8259.c
e8253.o : e8253.c e8253.h
	gcc -c e8253.c
jwd1797.o : jwd1797.c jwd1797.h e8259.h name.h
	gcc -c jwd1797.c
keyboard.o : keyboard.c keyboard.h
	gcc -c keyboard.c
video.o : video.c video.h
	gcc -c video.c
screen.o : screen.c video.h screen.h keyboard.h mainboard.h
	gcc -c screen.c `pkg-config --cflags --libs gtk+-3.0` -export-dynamic
debug.o : debug.c debug.h
	gcc -c debug.c
clean :
	rm z100 mainboard.o 8085.o 8088.o e8259.o e8253.o jwd1797.o keyboard.o video.o screen.o debug.o
