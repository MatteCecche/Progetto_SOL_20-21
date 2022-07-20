#!/bin/bash

LOG_FILE=./Filelog/filelog.txt
VERDE="\e[0;32m"
ORO="\e[0;33m"
BLU="\e[0;34m"
BIANCO="\e[0m"
BYTES=1024

echo -e "\n${BLU}# ---------------- Statistiche operazioni effettuate ---------------- #\n${BIANCO}"

n_reads=$(grep " Lettura " -c $LOG_FILE)
tot_bytes_read=$(grep "Lettura " ${LOG_FILE} | grep -Eo "[0-9]+" | { sum=0; while read num; do ((sum+=num)); done; echo $sum; } )
echo -e "${BIANCO}Numero di readFile: ${VERDE}${n_reads}${BIANCO}"
if [[ ${n_reads} > 0 ]]; then
    media_bytes_read=$(echo "scale=4; ${tot_bytes_read} / ${n_reads}" | bc -l)
    echo -e "${BIANCO}Size media delle letture in bytes: ${VERDE}${media_bytes_read}${BIANCO}"
  else
    echo -e "${BIANCO}Size media delle letture in bytes:${VERDE} 0 ${BIANCO}"
fi
n_writes=$(grep "Scritto" -c $LOG_FILE)
n_append=$(grep "Append sul file " -c $LOG_FILE)
tot_writes=$(( ${n_writes} + ${n_append} ))
tot_bytes_writes=$(grep "Scritto " ${LOG_FILE} | grep -Eo "[0-9]+" | { sum=0; while read num; do ((sum+=num)); done; echo $sum; } )
tot_bytes_append=$(grep "Append sul file " ${LOG_FILE} | grep -Eo "[0-9]+" | { sum=0; while read num; do ((sum+=num)); done; echo $sum; } )
echo -e "${BIANCO}Numero di writeFile: ${VERDE} ${tot_writes} ${BIANCO}"
if [[ ${tot_writes} > 0 ]]; then
    media_bytes_writes=$(echo "scale=4;( ${tot_bytes_writes} + ${tot_bytes_append} ) / ${tot_writes}" | bc -l)
    echo -e "${BIANCO}Size media della scrittura in bytes: ${VERDE}${media_bytes_writes}${BIANCO}"
  else
    echo -e "${BIANCO}Size media della scrittura in bytes:${VERDE} 0 ${BIANCO}"
fi
n_lock=$(grep " LOCK" -c $LOG_FILE)
echo -e "${BIANCO}Numero di Lock: ${VERDE}${n_lock}${BIANCO}"
n_openlock=$(grep "OPENLOCK" -c $LOG_FILE)
echo -e "${BIANCO}Numero di OpenLock: ${VERDE}${n_openlock}${BIANCO}"
n_unlock=$(grep "UNLOCK" -c $LOG_FILE)
echo -e "${BIANCO}Numero di UnLock: ${VERDE}${n_unlock}${BIANCO}"
n_close=$(grep " CLOSE " -c $LOG_FILE)
echo -e "${BIANCO}Numero di Close: ${VERDE}${n_close}${BIANCO}"
max_size=$(grep " bytes " ${LOG_FILE} | grep -Eo "[0-9]+")
max_size_m=$(echo "scale=4;( ${max_size} ) / ${BYTES}" | bc -l)
max_size_m=$(echo "scale=4;( ${max_size_m} ) / ${BYTES}" | bc -l)
echo -e "${BIANCO}Dimensione massima in Mbytes raggiunta dallo storage : ${VERDE}${max_size_m}${BIANCO}"
max_files=$(grep " massimo " ${LOG_FILE} | grep -Eo "[0-9]+")
echo -e "${BIANCO}Dimensione massima in numero di file raggiunta dallo storage : ${VERDE}${max_files}${BIANCO}"
max_algo=$(grep " Algoritmo " ${LOG_FILE} | grep -Eo "[0-9]+")
echo -e "${BIANCO}L’algoritmo di rimpiazzamento della cache è stato eseguito : ${VERDE}${max_algo}${BIANCO}"
max_n_rich=$(grep " processato " -c  ${LOG_FILE})
echo -e "${BIANCO}Numero di richieste servite da ogni worker thread : ${BIANCO}"

for (( i = 0; i < ${max_n_rich}; i++ )); do
  n=$(grep "Worker $i, ha processato" ${LOG_FILE} | grep -Eo "<[0-9]+>" | grep -Eo "[0-9]+")
  echo -e "${ORO}Il worker ${VERDE}[$i] ${ORO}ha processato ${VERDE}[$n] ${ORO}richieste${BIANCO}"
done;

echo -e "${BIANCO}Numero di richieste servite da ogni worker thread : ${VERDE}${max_n_rich}${BIANCO}"
max_conn=$(grep -Eo "Worker [0-9]+" ${LOG_FILE} | grep -Eo "[0-9]+" | { max=0; while read num; do if (( max<num )); then ((max=num)); fi; done; echo $max; } )
echo -e "${BIANCO}Massimo n. di connessioni contemporanee : ${VERDE}${max_conn}${BIANCO}"


echo -e "\n${BLU}# -------------------------------------------------------------------- #\n${BIANCO}"
