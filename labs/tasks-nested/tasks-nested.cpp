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

/**
 * the subtasks must complete before doSubTasks() completes
 */
void doSubTasks(int i) {
   auto tid = myThreadNum();
   int J = 2;
   for (int j=0;j<J;j++) {
     #pragma omp task untied
     {
        printf("doSubTasks() - %d:    subtask %d/%d on thread %d\n",
               i,(j+1),J,tid);
     }
   }
   #pragma omp taskwait
        printf("doSubTasks() - %d: completed on thread %d\n",i, tid);
}

/**
 * the subtasks are in a group that must complete before SubGroup() 
 * can continue (return)
 */
void doSubGroup(int i) {
   auto tid = myThreadNum();
   int J = 2;
   #pragma omp taskgroup
   {
      for (int j=0;j<J;j++) {
         #pragma omp task
         { 
            printf("doSubGroup() - %d:    subtask %d/%d on thread %d\n",
                   i,(j+1),J,tid);
         }
      }
   }
            printf("doSubgroup() - %d: completed on thread %d\n",i, tid);
}

/**
 * the subtasks can run independently of SomeTasks(). Meaning they 
 * can complete before/after SomeTasks()
 */
void doSomeTasks(int i) {
   auto tid = myThreadNum();
   int J = 2;
   for (int j=0;j<J;j++) {
     #pragma omp task untied
     {
        printf("doSomeTasks() - %d:    subtask %d/%d on thread %d\n",
          i,(j+1),J,tid);
     }
   }
        printf("doSomeTasks() - %d: completed on thread %d\n",i, tid);
}

/**
 * main
 */
int main(int argc, char** argv) {

   // number of groups
   int ngroups = 2;

   // for each thread 
   #pragma omp parallel
   {
      #pragma omp single
      {
         printf("*** number of threads: %d ***\n",numThreads());

         auto tid = myThreadNum();
         printf("*** thread %d creating tasks ***\n",tid);

         // CHOOSE: over or under
         auto over = numThreads() * 2;
         auto under = (int)(numThreads()/2);
         for (int i = 0; i < under; i++ ) {
            #pragma omp task
            {
               // CHOOSE: 1, 2, 3
               auto choice = 1;
               switch (choice) {
                  case 1:
                  // launch independent tasks and subtasks
                  doSomeTasks(i);
                  break;

                  case 2:
                  // launch tasks dependent on subtasks
                  doSubTasks(i);
                  break;

                  case 3:
                  default:
                  // launch tasks dependent on subtasks
                  doSubGroup(i);
                  break;
               } // choice
            } // omp
         } // for 

         // no task will start until we are past the single
         printf("*** thread %d done creating tasks ***\n",tid);

      } // other threads blocked here
   }

   printf("main done launching tasks\n");
} // main
