/**
 * Creating or assigning roles to threads. 
 *
 * If we were to create an OO design for a leader and a worker 
 * pool, the code/design could include three classes, a leader, 
 * worker(s), and maybe a pool. For threading either we inherit 
 * from Thread or run a class instance within a thread. Here is 
 * another approach using OpenMP.
 *
 * Let's look how to create this using OpenMP. We still have roles,
 * but let's take advantage of thread IDs to assign them.
 */

#include <iostream>
#include <queue>
#include "omp.h"

#include "task.hpp"
#include "worker.hpp"



/**
 * protect against compiling without -fopenmp
 */
int myThreadNum() {
   #ifdef _OPENMP
      return omp_get_thread_num();
   #else
      return 1;
   #endif
}

/**
 * protect against compiling without -fopenmp
 */
int numThreads() {
   #ifdef _OPENMP
      return omp_get_num_threads();
   #else
      return 1;
   #endif
}


int main(int argc, char** argv) {

   // the worker pool

   Worker<Task>* pool;

   // for each thread 
   #pragma omp parallel 
   {
      int nthreads = numThreads();

      // create and initialize the worker pool
      #pragma omp single
      {
         if ( nthreads == 1 ) {
            throw std::runtime_error("Minimum number of threads is 2.");
         }

         printf("\n** we have %d threads **\n\n",nthreads);
         pool = new Worker<Task>[nthreads];
      }

      int tid = myThreadNum();
      pool[tid].setId(tid);

      #pragma omp barrier

      if ( tid == 0 ) {
        // leader ---------------------------------------------------------------
	
         int numTasks = nthreads * 3;
         printf("%03d: I'm the leader, creating %d tasks\n\n",tid,numTasks);

         // create tasks
	 int w = 1; 
         for (int t=0; t < numTasks; t++) {
             Task tsk(t);
             pool[w].addWork(tsk);

	     // round-robin across workers
	     if (w < nthreads-1) w++;
	     else w = 1;
         }
      } else {
         // worker ---------------------------------------------------------------
	
          for (;;) {
             try {		  
                Task tsk = pool[tid].take();
                printf("Worker %03d executing task %03d.\n",tid,tsk.id());
                tsk.perform();
             } catch (const std::exception& ex) {
	        printf("worker %03d: error: %s.\n", tid, ex.what());
	        break;
             }
          }
      }

   } // end parallel
}
