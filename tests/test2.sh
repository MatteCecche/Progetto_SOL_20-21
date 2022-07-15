#!/bin/bash

./client  -f ./mysock                                                     \
-W ./Files/cane.jpg,./Files/mare.jpg                                      \
-D ./Espulsi                                                              \
-p                                                                        \
&

./client -f ./mysock                                                      \
-W ./OtherFiles/SubDir/juve.txt                                           \
-D ./Espulsi                                                              \
-a ./OtherFiles/SubDir/juve.txt,./OtherFiles/SubDir/juve.txt              \
-A ./Espulsi                                                              \
-p                                                                        \
&


./client  -f ./mysock                                                     \
-W ./OtherFiles/SubDir/spazio.jpg,./OtherFiles/SubDir/gatto.jpg           \
-D ./Espulsi                                                              \
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
