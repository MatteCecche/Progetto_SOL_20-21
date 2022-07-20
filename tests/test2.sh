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
-p                                                                              \
&


./client  -f ./mysock                                                           \
-w ./Files/Imm@O_CREATE_LOCK                                                    \
-D ./Espulsi                                                                    \
-W ./Cartella/SubCartella/capre.txt,./Cartella/SubCartella/rubinetto.txt        \
-D ./Espulsi                                                                    \
-p


if [ -e server.PID ]; then
    echo -e "${BIANCO}Chiusura server...${BIANCO}"

    kill -s SIGHUP $(cat server.PID)
    rm server.PID

    echo -e "${BIANCO} Chiuso${BIANCO}"
else
  echo -e "${BIANCO}Non ho trovato il pid del server${BIANCO}"
fi

sleep 2s

echo -e "\n\e[0;34m ---------- TEST 2 COMPLETATO ---------- \n\e[0m"
