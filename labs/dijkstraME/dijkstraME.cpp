/**
 * A direct implementation of Dijkstra's mutual exclusion (1965) 
 * algorithm as defined by in the book Distributed Algorithms, 
 * Nancy Lynch, 1996, sec 10.3.
 *
 * Tasks/Questions:
 * 0) Replace gotos.
 * 1) Replace threads with OpenMP.
 * 2) Replace threads with processes (ordinary and MPI) using 
 *    shared memory.
 * 3) 'turn' is both readable and writable by every process, 
 *    this can lead to race conditions. How do you fix this?
 * 4) How would you make this algo equally fair to all 
 *    processes?
 * 5) Other places you can strengthen the code?
 * 6) Sleep() was added for demonstration purposes. How does 
 *    sleep(n) and n affect the algo?
 *
 **/

#include <iostream>
#include <cstdlib>
#include <thread>

// ----------------------------------------------------- 

// how many threads (processes)
#define nThreads 8

// algo' state: None(0), Ask(1), Granted(2)
int flag[nThreads];

// critical code/section counting
int critical[nThreads];

// who is requesting (got) access
int turn = 3;

// verbosity of prints, more diagnostic output (0,1,2)
int verbose = 1;

// how many cycles to run the demonstration
int cycleMax = 20;

// ----------------------------------------------------- 


/**
 * The critical code section
 */
void criticalCode(int threadID, int cycle) {
  // count process (thread) accesses
  if ( threadID >= 0 && threadID < nThreads )
     critical[threadID] += 1; 

  if (verbose > 0) { 
     printf("cycle %3d: thread %d in critical code, cnt = %3d\n", cycle, 
            threadID, critical[threadID]);
  }
  
  // DEMO: computational weight
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

/**
 * emulates a process (thread) running dijkstraME, lecture 
 * psuedo-code demarcation : R, T, C, and E
 */
void process(int threadID) {

  // ========================================
  // Non-contended region (R) of code

  if ( verbose > 1 )
     printf("I am thread: %02d\n",threadID);

  // ----------------------------------------------
  // this is the dijkstra's algo

  // limit the time the demostration runs, replaces while(true)
  auto cycle = 0;
  while ( cycle < cycleMax ) {
     cycle++;

     L:

     // show interest in entering critical code
     flag[threadID] = 1;

     // ========================================
     // Try (T) region of algo

     if ( turn != threadID ) {
        // processes can request access to the critical code.

        flag[threadID] = 0;

        // if turn is done, try obtaining (race condition)
        if ( flag[turn] == 0 ) {
           turn = threadID;   
           if (verbose > 1) printf("thread %d is asking.\n",threadID);
        } else {
           // DEMO: Processes would loop (spin) in this state. For 
           // the demonstration, we want these threads to eventually 
           // exit the cycle loop.
   
           if ( cycle > cycleMax ) break;
           std::this_thread::sleep_for(std::chrono::milliseconds(5));
           cycle++;
        }

        goto L;
     }

     // seek grant (turn == threadID)
     if (verbose > 1) printf("thread %d seeking grant.\n",threadID);
     flag[threadID] = 2;

     // verify grant
     for ( int j = 0; j < nThreads ; j++ ) {
        // if any process other than self is at this state, reset
        if (j != threadID && flag[j] == 2) {
          if (verbose > 1) 
             printf("--- WHOA: thread %d grant conflict, %d retreating.\n",j,threadID);
          goto L;
        }
     }

     // ========================================
     // Critical (C) region of code

     criticalCode(threadID,cycle);

     // ========================================
     // Exit (C) region of code

     // process has exited critical code
     flag[threadID] = 0;

     // DEMO: Let other threads have a chance at entering critical code
     std::this_thread::sleep_for(std::chrono::milliseconds(50));

  } // cycle
  
}

int main (int argc, char** args) {

  // intialize 
  for (int i = 0 ; i < nThreads ; i++ ) {
     flag[i] = 0;
     critical[i] = 0;
  }

  // create/start threads
  std::vector<std::thread> threads(nThreads);
  for ( int t = 0 ; t < nThreads ; t++ ) {
     std::cerr << "creating thread " << t << " of " << nThreads << std::endl;
     threads[t] = std::thread(process, t); 
  }
  std::cerr << std::endl;

  // wait for all the threads to finish
  for ( auto& t : threads ) {
     t.join();
  }

  // print the results 
  auto sum = 0;
  std::cout << "\n\nResults: Processes/Threads gained access:" << std::endl;
  for ( int j = 0; j < nThreads ; j++ ) {
     sum += critical[j]; 
     std::cout << "process " << j << ": " << critical[j] << std::endl;   
  }

  std::cout << "\nGranted " << sum << " accesses in " << cycleMax 
            << " cycles." << std::endl;
}
