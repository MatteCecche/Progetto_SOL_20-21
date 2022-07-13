#!/bin/bash

./client  -f ./mysock                                                     \
-W ./Files/gif_imm.gif,./Files/gif_imm2.gif                               \
-D ./Espulsi                                                              \
-p                                                                        \
&

./client -f ./mysock                                                      \
-W ./OtherFiles/SubDir/LoremIpsum.txt                                     \
-D ./Espulsi                                                              \
-a ./OtherFiles/SubDir/LoremIpsum.txt,./OtherFiles/SubDir/LoremIpsum.txt  \
-A ./Espulsi                                                              \
-p                                                                        \
& 


./client  -f ./mysock                                                     \
-W ./OtherFiles/SubDir/pisa.jpg,./OtherFiles/SubDir/pianeti.jpg           \
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