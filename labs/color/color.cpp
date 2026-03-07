/**
 * Creating or assigning roles to threads. 
 *
 */

#include <iostream>
#include "omp.h"

#include "groups.hpp"


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

// (dis)enable to change approaches
#define APPROACH_ONE

int main(int argc, char** argv) {

   // number of groups
   int ngroups = 2;

#ifdef APPROACH_ONE
   printf("approach 1:\n\n");

   std::vector<Group> groups;
   groups.reserve(ngroups);
   for (auto i = 0 ; i < ngroups ; i++) {
     groups.emplace_back(Group(i));
   }
#else
   printf("approach 2:\n\n");
#endif

   // for each thread 
   #pragma omp parallel shared(groups)
   {
      auto nthreads = numThreads();

      // create and initialize things, if required
      #pragma omp single
      {
         if ( nthreads == 1 ) {
            throw std::runtime_error("Minimum number of threads is 2.");
         }

         printf("\n** we have %d threads **\n\n",nthreads);
      }

#ifdef APPROACH_ONE

      /* APPROACH 1 ---------------------------------------- */

      int tid = myThreadNum();
      int remainder = tid % ngroups;
      // printf("A1: thread %d, remainder %d\n",tid,remainder);

      #pragma omp critical
      groups[remainder].members().emplace_back(tid);

      // a join
      #pragma omp barrier

      // all threads are here now, we can do something!

      // to show groups
      #pragma omp single
      {
         for (auto& g : groups) {
            std::cout << "Group " << g.id() << ": ";
            for (auto t : g.members()) {
               std::cout << t << " ";
            }
            std::cout << std::endl;

         }
         std::cout << std::endl;
         std::cout.flush();
      }

#else
      /* APPROACH 2 ---------------------------------------- */

      int tid = myThreadNum();
      int remainder = tid % ngroups;

      if ( remainder == 0 ) {
         // do work for 0
         printf("thread %d is in group 0\n",tid);
      } else if ( remainder == 1 ) {
         // do work for 1
         printf("thread %d is in group 1\n",tid);
      } else if ( remainder == 2 ) {
         // do work for 2
         printf("thread %d is in group 2\n",tid);
      } else {
         // same group - do work for others 
      }

#endif

      // questions: ---------------------------------------- 
      // 0. Which approach do you prefer? 
      // 1. Limtations?
     
   } // end parallel
    
}
