# Galach

The Galach programming language. I made this to learn more about compiler development.  
I've made good progress with the bytecode compiler, but I've yet to make a virtual machine to execute the bytecode.  
There are some sample programs in the `programs/` directory, and you can compile them with `./galach programs/xxx.glc`  
  
The bytecode is described in `vm.h`. None of the produced bytecode is optimized (yet?)  
To ease testing, I made a simple disassembler, which is the output of the compiler for now.  

## Building
Building the compiler is pretty simple. I tried to keep most of the source in C99.  
To run some tests (defined in `tests.c`), you can build and run with `make MODE=test run`.  
To build debug, you can build and run with `make run`.
To build the optimized executable, you can build and run with `make MODE=prod run`

