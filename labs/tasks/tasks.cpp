/**
 * Creating or assigning roles to threads. 
 *
 */

#include <iostream>
#include "omp.h"

/**
 * protect against compiling without -fopenmp
 */
int myThreadNum() {
   #ifdef _OPENMP
      return omp_get_thread_num();
   #else
      return 0;
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

// placeholder for task
void doSomeStuff(int i) {
   auto tid = myThreadNum();
   printf("doSomeStuff() - task %d ran by thread %d\n",i, tid);
}

int main(int argc, char** argv) {

   // number of groups
   int ngroups = 2;

   // for each thread 
   #pragma omp parallel
   {
      auto nthreads = numThreads();

      #pragma omp single
      {
         auto tid = myThreadNum();
         printf("thread %d launching tasks\n",tid);

         for (int i = 0; i < 10; i++ ) {
            #pragma omp task
            {
               // launch your task
               doSomeStuff(i);
            }
         } 
         printf("thread %d done launching tasks\n",tid);
      } // other threads blocked here
   }

   printf("main done launching tasks\n");
} // main
