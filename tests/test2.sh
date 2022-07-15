#!/bin/bash

echo -e "\n\e[0;34m ---------- INIZIO TEST 2 ---------- \n\e[0m"

./client  -f ./mysock                                                           \
-W ./Files/cane.jpg,./Files/mare.jpg                                            \
-D ./Espulsi                                                                    \
-p                                                                              \
&

./client -f ./mysock                                                            \
-w ./Cartella/SubCartella                                                       \
-D ./Espulsi                                                                    \
-a ./Cartella/SubCartella/rubinetto.txt,./Cartella/SubCartella/rubinetto.txt    \
-A ./Espulsi                                                                    \
-r                                                                              \
-D ./Letti                                                                      \
-p                                                                              \
&


./client  -f ./mysock                                                           \
-W ./Cartella/SubCartella/spazio.jpg,./Cartella/SubCartella/capre.txt           \
-D ./Espulsi                                                                    \
-w ./Files/Imm                                                                  \
-D ./Espulsi                                                                    \
-w ./Cartella/SubCartella                                                       \
-p


if [ -e server.PID ]; then
    echo "Chiusura server..."

    kill -s SIGHUP $(cat server.PID)
    rm server.PID
    rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*

    echo "Chiuso"
else
  echo "Non ho trovato il pid del server"
fi

sleep 3s

echo -e "\n\e[0;34m ---------- TEST 2 COMPLETATO ---------- \n\e[0m"
