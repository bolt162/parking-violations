//#include <iostream>
#include <cstdio>
#include "omp.h"

#define NOT_THREADED -999

/**
 * number of cores to use
 */
int myThreads() { 
#ifdef _OPENMP
  return omp_get_thread_num();
#else
  return NOT_THREADED;
#endif
}


/**
 * @brief basic starting point - no threading
 *
 *      Author: gash
 */
int main(int argc, char **argv) {

   #pragma omp parallel
   {
     int ID = myThreads();
     printf("Hello (%d)\n",ID);
   }

}
