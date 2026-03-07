# Leader-Worker using OpenMP

This lab helps us understand how to assign roles to theads. In the lab we create a leader and a set 
of workers based on thread ID. The leader (thread 0) assigns work to the other threads (workers).

## Goals

Work with the code to see how you can (easy to more involved):

   * Why do we need the omp barrier in leader.cpp?
   * The leader.cpp file is really main.cpp. Modify the code to extract the leader from the main.
   * How would you modify the code to allow the leader's Worker to accept tasks?
   * How could the program gracefully shutdown and exit?
   * What are the implications that each worker has its own queue? 
       * How would you modify the code to have only one queue across all workers?
   * Tasks are distributed using a round robin method, what would happen if
     the amount of work for each task is not equal? Should 
     you change the code to take into account for this? If so, how?
   * The code (leader) pushes tasks to the worker threads. How can you modify the code to allow
     the workers to decide whether to accept work from the leader?


