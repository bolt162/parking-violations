/*
 * Ref:
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "defs.h"

int main()
{
    // Locate the segment. Fails if the server has not already 
    // created the segment!

    int shmid = shmget(APP_SHM_KEY, APP_SHM_SIZE, 0666);
    if ( shmid < 0 ) {
        perror("shmget failed to get application's shared memory segment.");
        exit(1);
    }

    // attach the segment to our data space.
    AppData *shm = (AppData *)shmat(shmid, NULL, 0);
    if ( shm == (AppData *)0 ) {
        perror("shmat did not attached to shared memory.");
        exit(1);
    }

    // read (print) what the server put in the memory.
    int lastVersion = -1;
    do {
        if ( lastVersion != shm->version ) {
          lastVersion = shm->version;
          printf("--> %02d: %s\n", shm->version, shm->payload);
        }

        // simulate processing delay
        sleep(1);
    } while( !shm->shutdown );

    // Was the results what you expected? What synchronization 
    // issues can you see with the do-while()?

    // ack shutdown request
    shm->ack = '*';

    // TODO cleanup
    sleep(2);
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    printf("client shutting down\n");
    exit(0);
}
