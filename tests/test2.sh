#!/bin/bash

./client  -f ./mysock                                                     \
-W ./Files/cane.jpg,./Files/mare.jpg                               \
-D ./Espulsi                                                              \
-p                                                                        \
&

./client -f ./mysock                                                      \
-W ./OtherFiles/SubDir/juve.txt                                     \
-D ./Espulsi                                                              \
-a ./OtherFiles/SubDir/juve.txt,./OtherFiles/SubDir/juve.txt  \
-A ./Espulsi                                                              \
-p                                                                        \
&


./client  -f ./mysock                                                     \
-W ./OtherFiles/SubDir/spazio.jpg,./OtherFiles/SubDir/gatto.jpg           \
-D ./Espulsi                                                              \
-p


if [ -e server.PID ]; then
    echo "Killing server..."

    kill -s SIGHUP $(cat server.PID)
    rm server.PID

    echo "Killed"
else
  echo "NO server pid found"
fi
