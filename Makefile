all: app.exe

ifeq ($(OS),Windows_NT)
CFLAGS = -ggdb -O3 -Wall -Wextra
LFLAGS = -ggdb -pthread
else
CFLAGS = -ggdb -O3 -Wall -Wextra -fsanitize=thread
LFLAGS = -ggdb -pthread -fsanitize=thread
endif

app.exe: main.o log.o Makefile
	gcc -o app.exe main.o log.o $(LFLAGS)

main.o: main.c log.h Makefile
	gcc -c main.c $(CFLAGS)

log.o: log.c log.h Makefile
	gcc -c log.c $(CFLAGS)

.PHONY: clean
clean:
	rm -f log.o main.o app.exe
