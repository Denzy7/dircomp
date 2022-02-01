# dircomp
Compress a directory with LZ4  

Build with cmake.  
`cmake -S . -B build`  
`cmake --build build`

# usage
`usage : dircomp [operation] [options]`  

operations:

- compress ( `-c [directory]` ) : Compress a directory  
	- `-o [file]` : set an output file to save compressed data

 - decompress( `-d [file]` ) : Decompress file to this directory

