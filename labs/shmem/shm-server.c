/*
 * Ref:
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "defs.h"

int main()
{
    printf("size of app data segment: %ld\n", APP_SHM_SIZE);

    // Create the segment. Note IPC_CREAT does not exist 
    // in shm-client.c!

    int shmid = shmget(APP_SHM_KEY, APP_SHM_SIZE, IPC_CREAT | IPC_EXCL | 0666);
    if ( shmid < 0) {
        perror("shmget");
        exit(1);
    }

    // Now we attach the segment to our data space.
    AppData *shm = (AppData*)shmat(shmid, NULL, 0);
    if (shm == (AppData *) -1 ) {
        perror("shmat");
        exit(1);
    }

    // initialize data - version number/counter
    shm->version = 0;
    shm->shutdown = false;
    shm->ack = 0;
    memset(shm->payload,0,APP_PAYLOAD_SIZE);

    // -------------------------------------------------------
    // DEMO: simulate evolving data. Using a sleep(),
    // 2 sec, to let the client check on updates. 

    char *s = shm->payload;
    for (char c = 'a'; c <= 'z'; c++) {
        // this write doesn't need to send results to the client 
        // as it is reading the same memory
        *s++ = c;

        shm->version += 1;  // inc version

        sleep(2);
    }

    // simulates the server continuing to work (create data)
    shm->shutdown = true;
    while (shm->ack != '*') {
        printf("working...waiting for client\n");
        sleep(1);
    }

    printf("\nclient acks shutdown\n");
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    exit(0);
}
