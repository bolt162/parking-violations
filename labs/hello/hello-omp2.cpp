#include <iostream>
#include <stdio.h>

#if defined (_OPENMP)
#include "omp.h"
#endif

/**
 * @brief basic starting point - adding openMP calls
 *
 *      Author: gash
 */
int main(int argc, char **argv) {

#if defined (_OPENMP)
   int numD = omp_get_num_devices();
   int numT = omp_get_num_threads();
   int maxT = omp_get_max_threads();
   int maxP = omp_get_num_procs();
#else
   int numD, numT, maxT, maxP;
   numD = numT = maxT = maxP = -1;
#endif

   printf("\nnum threads: %d\n",numT);
   printf("max threads: %d\n",maxT);
   printf("num devices: %d\n",numD);
   printf("num procs  : %d\n\n",maxP);
     
   #pragma omp parallel
   {
     int ID = -1;
#if defined (_OPENMP)
     ID = omp_get_thread_num();
#endif

     printf("Hello (%d)\n",ID);
   }

}
