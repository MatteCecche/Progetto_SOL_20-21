#!/bin/bash

./client                                                \
-f ./mysock                                             \
-t 200                                                  \
-W ./OtherFiles/ciao.txt                                \
-t 200                                                  \
-a ./OtherFiles/ciao.txt,./OtherFiles/ciao.txt          \
-t 200                                                  \
-A ./Espulsi                                            \
-t 200                                                  \
-r ./OtherFiles/ciao.txt                                \
-t 200                                                  \
-d ./Letti                                              \
-t 200                                                  \
-w ./Files                                              \
-t 200                                                  \
-D ./Espulsi                                            \
-t 200                                                  \
-W ./OtherFiles/SubDir/buongiorno.txt                   \
-t 200                                                  \
-R 1                                                    \
-t 200                                                  \
-d ./Letti                                              \
-t 200                                                  \
-l ./OtherFiles/SubDir/buongiorno.txt                   \
-t 200                                                  \
-u ./OtherFiles/SubDir/buongiorno.txt                   \
-t 200                                                  \
-c ./OtherFiles/SubDir/buongiorno.txt                   \
-t 200                                                  \
-R                                                      \
-t 200                                                  \
-d ./LettiFinal                                         \
-t 200                                                  \
-p

./client -h

echo "FATTO"


if [ -e server.PID ]; then
    echo "Chiusura server..."

    kill -s SIGHUP $(cat server.PID)
    rm server.PID
    rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*

    echo "Chiuso"
else
  echo "Non ho trovato il pid del server"
fi
