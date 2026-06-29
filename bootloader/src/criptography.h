#ifndef CRIPTOGRAPHY_H
#define CRIPTOGRAPHY_H

#include <stdint.h>
#include <stddef.h>

/*
 * Cifratura Speck 32/64 in modalita' CTR.
 *
 * In CTR la cifratura genera un "keystream" (flusso pseudocasuale) che viene
 * messo in XOR con i dati: cifrare e decifrare sono percio' la STESSA
 * operazione, quindi un'unica funzione copre entrambi i versi.
 *
 *   data  : buffer da (de)cifrare in-place
 *   len   : numero di byte da elaborare
 *   nonce : valore univoco per messaggio. Qui usiamo l'indirizzo della pagina
 *           flash: essendo diverso per ogni pagina, garantisce keystream diversi
 *           e rimuove la debolezza dell'XOR a chiave ripetuta (pagine uguali ->
 *           cifrati uguali). Il nonce e' pubblico per definizione del CTR.
 */
void CRI_crypt(uint8_t *data, uint16_t len, uint16_t nonce);

#endif // CRIPTOGRAPHY_H
