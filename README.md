# memprofile

A dynamic library for profiling allocations in an application

It does this by loading a library dynamically, without any need to instrument the application itself.

On MacOS:

    $ DYLD_INSERT_LIBRARIES=libmemprofile.dylib ./a.out

On Linux

    $ LD_PRELOAD=libmemprofile.dylib ./a.out


And the output will look like this:

    $ DYLD_INSERT_LIBRARIES=libmemprofile.dylib ./a.out
    Memory used: 
       Total:   512 bytes in 2 allocation(s)
       At exit: 0 bytes in 0 allocation(s)


## Known issues:

Currently only tested on MacOS

Currently, in my examples, there's always an initial ``realloc`` call of 256 bytes.
I haven't traced the location of this call yet, please let me know if you know where it comes from :)