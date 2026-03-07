/**
 * A direct implementation of Lamport's Bakery Mutual Exclusion 
 * algorithm as defined by in the book Distributed Algorithms, 
 * Nancy Lynch, 1996, sec 10.3.
 *
 * See:
 * https://lamport.azurewebsites.net/pubs/bakery/dbakery-complete.pdf
 *
 * The Bakery algorithm differs from Djikstra's 'turn' as
 * a multi-writer/multi-reader field, to a single-writer/
 * multi-reader (distinct for each process). Second, the
 * bakery algo. is fair.
 *
 * Notes on example:
 * 0) This is a simulation of many processes s.t. a thread represents 
 *    a process. Do not confuse a process is a thread.
 * 1) Sleep() are added for simulation of CPU resistance (load).
 * 2) To simplfy tje example, global variables are used instead 
 * of shared memory.
 *
 * Tasks/Questions:
 * 0) What is the convergence impact from changing Djikstra's 
 * 'turn' to an array (per process index)?
 * 1) Replace threads with processes (ordinary and MPI) using 
 *    shared memory.
 * 2) How would you make this algo fair to all processes?
 * 3) How would you modify this algo to work across nodes?
 * 4) How would you add a time limit to the crictical section? 
 *    Consequences? Why?
 **/

#include <iostream>
#include <cstdlib>
#include <limits.h>
#include <thread>

// ----------------------------------------------------- 

// how many threads (processes)
#define nThreads 8

// Demo: verbosity of prints, more diagnostic output (0,1,2,3,4)
int verbose = 1;

// Demo: cycle (in crit code) counting for the demonstration
int cycleMax = 10;
int cycle = 0;

// Demo: critical code/section counting
int critical[nThreads];

// shm: coordinate choosing a number
int choosing[nThreads];

// shm: what numbers do the other processes have
int number[nThreads];

// ----------------------------------------------------- 

/**
 * get a number (ticket) base on the maximum number
 * held by all processes (threads). Note iterating 
 * over the numbers held does not prevent duplicate
 * numbers. The algo itself has support for resolving
 * ties.
 */
int getNumber() {
  auto max = 1;
  for (int i=0; i < nThreads; i++) {
     if ( number[i] >= max ) {max = number[i] + 1;}
  }
    
  return max;
}

/**
 * The critical code section
 */
void criticalCode(int threadID) {
  // count process (thread) accesses
  if ( threadID >= 0 && threadID < nThreads )
     critical[threadID] += 1; 

  if (verbose > 0) { 
     printf("cycle %3d: thread %d in critical code, cnt = %3d\n", cycle, 
            threadID, critical[threadID]);
  }

  cycle++;
  
  // DEMO: computational weight
  //std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

/**
 * emulates a process (thread) running bakeryME (note
 * psuedo-code demarcation : R, T, C, and E)
 */
void process(int threadID) {

  // ========================================
  // Non-contended region (R) of code

  if ( verbose > 1 )
     printf("I am thread: %02d\n",threadID);

  // ----------------------------------------------
  // this is the bakery algo

  // DEMO: limit the time the demostration runs, replaces while(true)
  while ( cycle < cycleMax ) {

     // ========================================
     // Rem (R) region of algo

     // DEMO: assume this is the section of code not requiring 
     // access to the critical section.
     std::this_thread::sleep_for(std::chrono::milliseconds(1000));

     // ========================================
     // Try (T) region of algo

     /** 
      * using the bakery as a physical store analogy - allow only 
      * one person (process) at a time to enter the bakery's door 
      * to get a number.
      *
      * this implementation blocks while others are choosing a 
      * number. What would a wait free version look like?
      **/
     
     while (number[threadID] == 0) {
        auto can = true; 
        for (int i=0; i < nThreads; i++) {
             if (choosing[i] == 1) {can = false; break;}
        }    

        if ( can ) {
           choosing[threadID] = 1;
           number[threadID] = getNumber(); 

           if (verbose > 1) 
              printf("thread %2d got a number (%03d) on cycle %03d.\n",threadID,
                     number[threadID],cycle);

           choosing[threadID] = 0;
        } else {
           if (verbose > 2) { 
              printf("thread %2d waiting to choose a number on cycle %03d.\n",
                     threadID, cycle);
           }
        }
     }

     // very chatty
     if (verbose > 3) {
        if (threadID == 0) {
           std::this_thread::sleep_for(std::chrono::milliseconds(2000));
           printf("--- Numbers, cycle: %3d\n",cycle);
           for ( int i=0; i < nThreads; i++ ) {
              printf("    thread %3d: %3d\n",i,number[i]);
           }
        } else {
           std::this_thread::sleep_for(std::chrono::milliseconds(4000));
        }
     }


     // ensure no processes are choosing a number.
     auto waitChoosing = true;
     while (waitChoosing) {
        waitChoosing = false;
        for ( int i=0; i < nThreads; i++ ) {
           if ( choosing[i] == 1 ) {waitChoosing = true; break;}
        }
 
        // DEMO: slow down the waiting
        if (verbose > 2 && waitChoosing) {
           printf("thread %2d waiting for other processes to choose a number.\n",threadID);
           std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
     }

     // ensure process has the lowest number. Processes that have
     // the same number, the process (thread) with the lowest ID
     // goes first. Iterate through ALL of the numbers (no break 
     // like above).

     auto waitLowest = true;
     while (waitLowest) {
        auto lowest = threadID;
        for ( int i=0; i < nThreads; i++ ) {
           if ( number[i] != 0 && number[i] < number[threadID] )
              lowest = i;
        }

        // if there are duplicate numbers, only the process with 
        // the lowest ID will advance.
        if (lowest == threadID) waitLowest = false;
        else { 
           // DEMO: slow down waiting for my number
           std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
     }

     // ========================================
     // Critical (C) region of code

     criticalCode(threadID);

     // ========================================
     // Exit (E) region of code

     // process has exited critical code. Setting the number to zero (0) 
     // is signaling to the other processes that it is no longer trying
     // to obtain the critical section. What happens if a process does not
     // exit the region?
     number[threadID] = 0;

     // Is this needed?
     //std::this_thread::sleep_for(std::chrono::milliseconds(10));

  } // cycle
  
}

int main (int argc, char** args) {

  // intialize 
  for (int i = 0 ; i < nThreads ; i++ ) {
     choosing[i] = 0;
     number[i] = 0;
     critical[i] = 0;
  }

  // create/start threads (proceses)
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
