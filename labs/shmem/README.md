# Shared Memory Synchronization

This example demonstrates the basic configuration (setup) of
a Shared Memory (SM) space, and the use of the one-way 
pattern for SM.

As discussed, SM is a the ability to (near) instantaneously 
share data between processes on the same node without
having to communicate through a socket or pipe.

## One-Way

The one-way pattern is the simplest form of sharing data in
a single producer (writer) provides data to one or more 
consumers (readers). While the code at first blush is fairly
straight forward, there are some valid questions that you 
should be prepared to answer in its use.

   * Organization - How do you represent complex data?
   * Updates - How are data updated and notifications 
     conveyed to the readers? What nuances should you
     be on the look out for when data is updated?
   * Beyond a node - What does multi-node SM look like?
   * What would it take to make this the code production 
     ready?


## Design

The client and server demonstrates the following:

   * A simple monotonically increasing version 
     number to indicate there has been a change 
     in the data. 
   * The payload, it has been simplified for the 
     demonstration.  
   * Client's acknowledgement when ending data 
     sharing (with the character '*' in the 
     ack field).

Note these features are for demonstration and do not 
represent hardened solutions. 


## Building

The example does not require additional libraries or 
compiler flags to build so, a simple clang or gcc is 
all that is required.

Two compiling examples are provided, 1) from the 
command line with gcc, and 2) with Cmake.

### Command line

 To compile the server:

```
 gcc shm-server.c -o shm-server
 gcc shm-client.c -o shm-client
```

### Cmake

Be sure to modify the CMakeLists.txt to specify your compiler

```
> mkdir build
> cd buld
> cmake ..
> make
```

## Running

 In two separate terminals (xterm) start the server first then the client. Note
 if the client is started first, the process will fail as the shared memory
 segment has not been created by the server.
