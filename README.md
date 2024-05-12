## Multi Threaded HTTP Server

### Description

The main objectives with this assignment are to practice implementing a client-server system with a thread-safe queue and rwlocks. Thus, we will be allowed to process multople clients simultaneously. You will see how we accomplish this when building our HTTP server in C in my program--*httpserver.c*--that takes our port to start our server and the number of threads to use.

Note that this project was a class programming assignment for Computer Systems Design.

### Author

*Created By:* Camille Gandotra  

### Usage

When downloaded we can use our *httpserver.c* program like so (in terminal; make sure you are in the right directory):

For *httpserver.c*:
```
./httpserver [-t threads] <port>

OPTIONS
    -t threads       Number of Worker Threads (Default: 4) (Optional)
    -port            Port Number (Number Between 1-65535)  (Required)
```

My server will now execute “forever” without crashing (runs untill CTRL-C)

If you are having trouble running the program, refer to the commands below.

For *Makefile*:

The following command builds *httpserver*:
```
make
```

More Options:
```
make [options]

OPTIONS:
     :                      Builds the httpserver program.
    clean :                 Removes all binaries.
    format :                Clang Formats our C files.

```

### Files

- ```httpserver.c``` - C program that contains the main() function for this assignment.
- ```asgn2_helper_funcs.h``` - Assignment 2 helper functions for main().
- ```asgn4_helper_funcs.a``` - Helper functions implementation for main().
- ```connection.h``` - Connection helper functions for main().
- ```queue.h``` - Queue helper functions for main().
- ```request.h``` - Request helper functions for main().
- ```response.h``` - Response helper functions for main().
- ```rwlock.h``` - Request helper functions for main().
- ```Makefile``` - Directs the compilation process. Able to build our main program *httpserver*. Able to clean binaries and format code.
- ```README.md``` - Description of the assignment and files provided. Demonstrates how to input arguments and what to expect for the output.
