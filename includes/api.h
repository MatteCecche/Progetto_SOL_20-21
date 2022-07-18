#ifndef API_H
#define API_H

// ------------------------------------------------------------------------------------------------------------------ //
// Apre una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la                   //
// richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo   //
// scadere del tempo assoluto ‘abstime’                                                                               //
// ------------------------------------------------------------------------------------------------------------------ //

int openConnection(const char* sockname, int msec, const struct timespec abstime);

// ----------------------------------------------------------------- //
// Chiude la connessione AF_UNIX associata al socket file sockname   //
// ----------------------------------------------------------------- //

int closeConnection(const char* sockname);

// ---------------------------------------------------------------------------------------- //
// Richiesta di apertura o creazione del file identificato da pathname (con o senza lock)   //
// ---------------------------------------------------------------------------------------- //

int openFile(const char* pathname, int flags, const char* dirname);

// ---------------------------------------------------------------------------------- //
// Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore   //
// ad un'area allocata sullo heap nel parametro ‘buf’, mentre ‘size’ conterrà         //
// la dimensione del buffer dati (ossia la dimensione in bytes del file letto)        //
// ---------------------------------------------------------------------------------- //

int readFile(const char* pathname, void** buf, size_t* size);

// ---------------------------------------------------------------------------------- //
// Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella          //
// directory ‘dirname’ lato client. Se il server ha meno di ‘N’ file disponibili,     //
// li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file    //
// memorizzati al suo interno                                                         //
// ---------------------------------------------------------------------------------- //

int readNFiles(int N, const char* dirname);

// ------------------------------------------------------------------------------------ //
//  Scrive tutto il file puntato da pathname nel file del server                        //
//  Ritorna successo solo se la precedente operazione, terminata con successo,          //
//  è stata openFile(pathname, O_LOCK). Se ‘dirname’ è diverso da NULL,                 //
//  i file eventualmente spediti dal server perchè espulsi dalla cache per far posto    //
//  al file ‘pathname’ dovranno essere scritti in ‘dirname’                             //
// ------------------------------------------------------------------------------------ //

int writeFile(const char* pathname, const char* dirname);

// ------------------------------------------------------------------------------------------------ //
//  Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’    //
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi          //
//  dalla cache per far posto ai nuovi dati di ‘pathname’ dovranno essere scritti in ‘dirname’      //
// ------------------------------------------------------------------------------------------------ //

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

// ---------------------------------------------------------------------------------------------- //
//  Setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la      //
//  richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato,    //
//  l’operazione termina immediatamente con successo, altrimenti l’operazione non                 //
//  viene completata fino a quando il flag O_LOCK non viene resettato dal detentore della lock    //
// ---------------------------------------------------------------------------------------------- //

int lockFile(const char* pathname);

// -------------------------------------------------------------------------------- //
//  Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se    //
//  l’owner della lock è il processo che ha richiesto l’operazione, altrimenti      //
//  l’operazione termina con errore                                                 //
// -------------------------------------------------------------------------------- //

int unlockFile(const char* pathname);

// ------------------------------------------------------------------ //
//  Richiesta di chiusura del file puntato da ‘pathname’              //
//  (Eventuali operazioni sul file dopo la closeFile falliscono)      //
// ------------------------------------------------------------------ //

int closeFile(const char* pathname);

// ------------------------------------------------------------------------------------- //
//  Rimuove il file cancellandolo dal file storage server.                               //
//  L’operazione fallisce se il file non è in stato locked, o è in stato locked          //
//  da parte di un processo client diverso da chi effettua la removeFile                 //
// ------------------------------------------------------------------------------------- //

int removeFile(const char* pathname);

// ---------------------------------------------------------------------------- //
// Legge dal socket 'fd' 'nfiles' files e li scrive in dirname(se != NULL)      //
// ---------------------------------------------------------------------------- //


#endif
