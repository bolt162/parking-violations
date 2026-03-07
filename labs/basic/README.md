# Introduction to C/C++ and using CMake to build projects

Hello, this is a simple project that uses CMake to build executables. The 
intended purpose of this is to help you become acustomed to C++ and how
to build libraries and executables. 


## You will need

For this lab your laptop will need to install several packages. How you 
install can be a lab in of itself (using autoconf, building, deploying).

These packages will be required:

  * A C/C++ compiler (gcc, clang). While xcode (MAC OSX) is needed 
    to bootstrap building, please try to use gcc or clang.
  * CMake. A build tool similar to imake, and maven.
  * An editor (IDE for C++, vscode, neovim/vim, nano, etc)


## Steps to build

   1. Create a directory for building, anywhere you want, usually this is 
      outside of the source directory as we don't want to introduce 
      artifacts into our git repo. However, for demonstration purposes 
      let's create one in the lab's directory ('b' is great because it 
      is less typing): 

      <code>mkdir b</code>

   2. cd to the created directory: 

      <code>cd b</code>

   3. Create the make files by specifying where the CMakeLists.txt can
      be found. At this point you will want to choose which compiler to use (xcode, clang/llvm, gcc, etc.) Cmake will choose the compiler that is defined by CC and CXX environment variables.

      <code>env | grep CC</code> or <code>echo $CC</code>

      For instance to set CC and CXX:

      <code>export CC=/opt/homebrew/Cellar/llvm/21.1.8/bin/clang</code>
      <code>export CXX=/opt/homebrew/Cellar/llvm/21.1.8/bin/clang++</code>

      Or you can use Lmod which is a lot easier:

      <code>module load clang</code>
      
      Now let's create the makefile. In our example it is the parent directory (..):

      <code>cmake ..</code>

   4. Make (Build) the project. Executables and libraries are in the src and 
      test directories as instructed in the CMakeLists.txt

      <code>make</code>


## Where to go from here

Well, there are a lot of topics to jump from this introduction that will
be discussed in class. Working on the C/C++ will help you be prepared 
when we start. Some suggestions on what to work on:

   * Adding Boost headers and/or libraries
   * Building a library and linking it into another project (code)
   * Clang's static analyzer (checker)
   * Exploring newest features (20, 17, 14, 11), it takes awhile for 
     standards to be implemented so, not all features are available. 

     ** Move semantics
     ** Lambda support
     ** loops, templates, namespaces, etc


## Reading

C/C++
  * https://www.cplusplus.com/doc/tutorial/
  * https://www.geeksforgeeks.org/c-plus-plus/?ref=ghm
  * https://github.com/AnthonyCalandra/modern-cpp-features

Cmake
  * https://cmake.org/cmake/help/latest/guide/tutorial/index.html 
  * https://cliutils.gitlab.io/modern-cmake/README.html 
