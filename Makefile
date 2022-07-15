CC		=  gcc
CFLAGS  = -w
FLAGS			= -std=c99 -Wall

#default
all: server client


clean:
	rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*


# ------------------------------------------------------------------------------------------------------------------ SERVER ------------------------------------------------------------------------------------------------------------------ #

server: server.o coda.o fun_descrit.o coda_storage.o config.o signal_handler.o int_con_lock.o
	$(CC) -pthread server.o coda.o fun_descrit.o coda_storage.o config.o signal_handler.o int_con_lock.o -o server $(FLAGS)

server.o: ./src/server/server.c ./includes/def_tipodato.h
	$(CC) $(CFLAGS) -c ./src/server/server.c $(FLAGS)

coda.o: ./src/server/coda.c ./includes/coda.h ./includes/util.h ./includes/config.h
	$(CC) $(CFLAGS) -c ./src/server/coda.c $(FLAGS)

coda_storage.o: ./src/server/coda_storage.c ./includes/coda_storage.h ./includes/def_tipodato.h ./includes/util.h ./includes/coda.h ./includes/config.h ./includes/fun_descrit.h
	$(CC) $(CFLAGS) -c ./src/server/coda_storage.c $(FLAGS)

int_con_lock.o: ./src/server/int_con_lock.c ./includes/int_con_lock.h ./includes/util.h
	$(CC) $(CFLAGS) -c ./src/server/int_con_lock.c $(FLAGS)

fun_descrit.o: ./src/server/fun_descrit.c ./includes/fun_descrit.h
	$(CC) $(CFLAGS) -c ./src/server/fun_descrit.c $(FLAGS)

config.o: ./src/server/config.c ./includes/config.h ./includes/def_tipodato.h
	$(CC) $(CFLAGS) -c ./src/server/config.c $(FLAGS)

signal_handler.o: ./src/server/signal_handler.c ./includes/signal_handler.h ./includes/config.h
	$(CC) $(CFLAGS) -c ./src/server/signal_handler.c $(FLAGS)


# ------------------------------------------------------------------------------------------------------------------ CLIENT ------------------------------------------------------------------------------------------------------------------ #

client: client.o fun_descrit.o oper_coda.o libapi.a
	$(CC) client.o fun_descrit.o oper_coda.o -o client -L . -lapi $(FLAGS)

client.o: ./src/client/client.c ./includes/def_tipodato.h ./includes/def_client.h ./includes/util.h
	$(CC) $(CFLAGS) -c ./src/client/client.c $(FLAGS)

libapi.a: api.o
	ar rvs libapi.a api.o

api.o: ./src/client/api.c ./includes/api.h ./includes/def_tipodato.h
	$(CC) $(CFLAGS) -c ./src/client/api.c $(FLAGS)

oper_coda.o: ./src/client/oper_coda.c ./includes/oper_coda.h ./includes/def_tipodato.h ./includes/util.h ./includes/def_client.h
	$(CC) $(CFLAGS) -c ./src/client/oper_coda.c $(FLAGS)




# ------------------------------------------------------------------------------------------------------------------ TEST 1 ------------------------------------------------------------------------------------------------------------------ #

test1:
	valgrind --leak-check=full ./server -c ./Config/test1.conf & echo $$! > server.PID \
	& bash tests/test1.sh


# ------------------------------------------------------------------------------------------------------------------ TEST 2 ------------------------------------------------------------------------------------------------------------------ #

test2:
	./server -c ./Config/test2.conf & echo $$! > server.PID \
	& bash tests/test2.sh


# ------------------------------------------------------------------------------------------------------------------ TEST 3 ------------------------------------------------------------------------------------------------------------------ #

test3:
	./tests/test3.sh

# ---------------------------------------------------------------------------------------------------------------- STATISTICHE ---------------------------------------------------------------------------------------------------------------- #

stat:
	./Scripts/statistiche.sh
