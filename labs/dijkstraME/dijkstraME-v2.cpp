/**
 * An implementation of Dijkstra's mutual exclusion (1965) based
 * on the paper by ().
 *
 * Ref: An interesting approach/coding
 * https://www.eecs.yorku.ca/course_archive/2007-08/W/6117/DijMutexNotes.pdf
 * See tasks and questions in Lynch's implementation.
 **/

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <thread>

// how many threads (processes)
#define nThreads 8

// algo controls: interested, passed
bool interested[nThreads];
bool passed[nThreads];

// critical section counting
int critical[nThreads];

// who is initially asking access
int ask = 3;

// how many cycles to run the demo
int demoMax = 20;

// print more diagnostic output (0,1,2)
int verbose = 1;

/**
 * emulates a process (thread) accessing the critical section
 */
void criticalCode(int threadID, int cycle) {
  // count which process (thread) gains access
  if ( threadID >= 0 && threadID < nThreads )
     critical[threadID] += 1; 

  if (verbose > 0) { 
     printf("cycle %02d: thread %d ran critical code, cnt = %d\n", cycle, 
            threadID, critical[threadID]);
  }
}

/**
 * emulates a process (thread) accessing the critical section
 */
void process(int threadID) {
  if ( verbose > 1 )
     printf("I am thread: %02d\n",threadID);

  // ----------------------------------------------
  // this is the dijkstra's algo

  // limit the time the demonstration runs, replaces while(true)
  auto demo = 0;
  while ( demo < demoMax ) {
     demo++;

     L:
     interested[threadID] = false;

     if ( ask != threadID ) {
        // non-asking processes are indicating they pass on 
        // requesting access to the critical code. However,
        // if their state changes to wanting access

        passed[threadID] = true;
        if ( interested[ask] ) ask = threadID;   

        // original: the passing processes would loop (spin) forever in 
        // this state but, for the demo we want these threads to
        // eventually exit the demo loop.

        if ( demo > demoMax ) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        demo++;

        goto L;
     }

     passed[threadID] = false;

     for ( int j = 0; j < nThreads ; j++ ) {
        // if any process other than self is at this state, reset
        if ( j != threadID && !passed[j]) {
          if (verbose > 1) printf("another thread (%d) is trying to access, resetting.\n",j);
          goto L;
        }
     }

     // --------------------------------------------------------- 
     // a process doesn't reach this point unless it has access

     criticalCode(threadID,demo);

     passed[threadID] = true;
     interested[threadID] = true;

     std::this_thread::sleep_for(std::chrono::milliseconds(250));

  } // demo
  
}

int main (int argc, char** args) {

  // intialize 
  for (int i = 0 ; i < nThreads ; i++ ) {
     interested[i] = false;
     passed[i] = false;
     critical[i] = 0;
  }

  // create threads
  std::vector<std::thread> threads(nThreads);
  for ( int t = 0 ; t < nThreads ; t++ ) {
     std::cout << "creating thread " << t << " of " << nThreads << std::endl;
     threads[t] = std::thread(process, t); 
  }
  std::cout << std::endl;
  std::cout.flush();

  // wait for all the threads to finish
  for ( auto& t : threads ) {
     t.join();
  }

  // print the results of which thread got into 
  // the critical code section

  std::cerr << "\n\nResults: How many times a process gained access" << std::endl;
  for ( int j = 0; j < nThreads ; j++ ) {
     std::cerr << "process " << j << ": " << critical[j] << std::endl;   
  }
  
}
