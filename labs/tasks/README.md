# Defining roles & organization using OpenMP

This lab shows a technique to assign roles using thead IDs. It 
can also be adapted to support grouping processes on and off
nodes (more on this later). This lab shows how to group threads 
into groups. 

Imagine the two groups working together in a double-buffering
design to manage loading and processing data. What other 
applications can you consider this useful for?

## Design

Two approaches are presented. How would you evaluate them?

## Goals

Observe how threads can be divided into multiple groups.

   * How can the Group instances coordinate between other 
     groups?
   * Modify the code to have change how threads are divided.
     For instance, more loaders than processors.
   * What other use cases can you imagine this design 
     would be useful? 
   * How would you improve the design? Edge cases?
   * What are the advantage and/or drawbacks of these 
     approaches?



