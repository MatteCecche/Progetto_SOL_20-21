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
-r                                                                              \
-D ./Letti                                                                      \
-p                                                                              \
&


./client  -f ./mysock                                                           \
-w ./Files/Imm                                                                  \
-W ./Cartella/SubCartella/spazio.jpg,./Cartella/SubCartella/capre.txt           \
-D ./Espulsi                                                                    \
-D ./Espulsi                                                                    \
-w ./Cartella/SubCartella                                                       \
-p


if [ -e server.PID ]; then
    echo -e "${BIANCO}Chiusura server...${BIANCO}"

    kill -s SIGHUP $(cat server.PID)
    rm server.PID
    #rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*
    echo -e "${BIANCO} Chiuso${BIANCO}"
else
  echo -e "${BIANCO}Non ho trovato il pid del server${BIANCO}"
fi

sleep 2s

echo -e "\n\e[0;34m ---------- TEST 2 COMPLETATO ---------- \n\e[0m"
