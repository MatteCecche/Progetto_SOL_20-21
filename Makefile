CC			= gcc
CFLAGS  = -w


server: server.o config.o
				$(CC) server.o config.o -o server.out

config.o: config.c config.h
					$(CC) $(CFLAGS) -c config.c

test: config.o server
			@echo "Eseguo test"
			./server.out -c config.txt
			@echo "Test superato"
