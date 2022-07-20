#!/bin/bash
echo -e "\n\e[0m ---------- INIZIO TEST 3 ---------- \n\e[0m"

./server -c ./Config/test3.conf &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 2s

echo -e "30 secondi all'invio del segnale SIGINT"

sleep 2s

# faccio partire gli script per generare i client
array_id=()
for i in {1..10}; do
	bash -c './Scripts/LoopClient.sh' &
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

sleep 2s
kill -s SIGINT ${SERVER_PID}
sleep 2s

wait $SERVER_PID
killall -q ./client #chiudo eventuali processi rimasti
echo -e "\n\e[0m ---------- TEST 3 COMPLETATO ---------- \n\e[0m"
