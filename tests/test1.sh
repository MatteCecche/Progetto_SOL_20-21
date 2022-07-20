#!/bin/bash
BIANCO="\e[0m"
echo -e "\n\e[0;34m ---------- INIZIO TEST 1 ---------- \n\e[0m"

./client                                                          \
-f ./mysock                                                       \
-t 200                                                            \
-W ./Cartella/informatica.txt@O_CREATE_LOCK                       \
-t 200                                                            \
-a ./Cartella/informatica.txt,./Cartella/informatica.txt          \
-t 200                                                            \
-A ./Espulsi                                                      \
-t 200                                                            \
-r ./Cartella/informatica.txt                                     \
-t 200                                                            \
-d ./Letti                                                        \
-t 200                                                            \
-w ./Files                                                        \
-t 200                                                            \
-D ./Espulsi                                                      \
-t 200                                                            \
-W ./Cartella/SubCartella/capre.txt                               \
-t 200                                                            \
-R 1                                                              \
-t 200                                                            \
-d ./Letti                                                        \
-t 200                                                            \
-l ./Cartella/SubCartella/capre.txt                               \
-t 200                                                            \
-u ./Cartella/SubCartella/capre.txt                               \
-t 200                                                            \
-c ./Cartella/SubCartella/capre.txt                               \
-R                                                                \
-t 200                                                            \
-d ./LettiFinal                                                   \
-t 200                                                            \
-p

./client -h

echo -e "${BIANCO}FATTO${BIANCO}"


if [ -e server.PID ]; then
    echo -e "${BIANCO}Chiusura server...${BIANCO}"

    kill -s SIGHUP $(cat server.PID)
    rm server.PID

    echo -e "${BIANCO}Chiuso${BIANCO}"
else
  echo -e "${BIANCO}Non ho trovato il pid del server${BIANCO}"
fi

sleep 2s

echo -e "\n\e[0;34m ---------- TEST 1 COMPLETATO ---------- \n\e[0m"
