#!/bin/bash
echo -e "\n\e[0;34m ---------- INIZIO TEST 3 ---------- \n\e[0m"

./server -c ./Config/test3.conf &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 2s

bash -c 'sleep 30 && kill -2 ${SERVER_PID}' &
echo -e "30 secondi all'invio del segnale SIGINT"

# faccio partire gli script per generare i client
array_id=()
for i in {1..10}; do
	bash -c './Scripts/startClient.sh' &
	array_id+=($!)
	sleep 0.1
done
# Aspetto 30 secondi
sleep 30s
# stop processi
for i in "${array_id[@]}"; do
	kill "${i}"
	wait "${i}" 2>/dev/null
done

wait $SERVER_PID
killall -q ./client #chiudo eventuali processi rimasti
rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*
echo -e "\n\e[0;34m ---------- TEST 3 COMPLETATO ---------- \n\e[0m"
