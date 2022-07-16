while true
do
./client -f ./mysock                                                    \
-W ./Files/Imm/imm1.jpg,./Files/Imm/imm4.jpg,./Files/Imm/imm5.jpg 			\
-R 																								                      \
-W ./Files/cane.jpg,./Files/mare.jpg                      							\
-r cane.jpg, mare.jpg              																			\
-c ./Files/Imm/imm1.jpg, ./Files/Imm/imm4.jpg, ./Files/Imm/imm5.jpg     \
-W ./Cartella/informatica.txt                                           \
-a ./Cartella/informatica.txt,./Cartella/informatica.txt                \
-r ./Cartella/informatica.txt                                           \
-c ./Cartella/informatica.txt
done
