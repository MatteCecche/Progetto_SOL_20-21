#!/bin/bash

./client                     \
-f ./mysock                                             \
-t 200                                                  \
-W ./OtherFiles/ciao.txt                            \
-a ./OtherFiles/ciao.txt,./OtherFiles/ciao.txt  \
-A ./Espulsi                                            \
-r ./OtherFiles/ciao.txt                            \
-d ./Letti                                              \
-w ./Files                                              \
-D ./Espulsi                                            \
-W ./OtherFiles/SubDir/buongiorno.txt                     \
-R                                                      \
-d ./Letti                                              \
-l ./OtherFiles/SubDir/buongiorno.txt                     \
-u ./OtherFiles/SubDir/buongiorno.txt                     \
-c ./OtherFiles/SubDir/buongiorno.txt                     \
-R                                                      \
-d ./LettiFinal                                         \
-p

echo "DONE"


if [ -e server.PID ]; then
    echo "Killing server..."

    kill -s SIGHUP $(cat server.PID)
    rm server.PID

    echo "Killed"
else
  echo "NO server pid found"
fi
