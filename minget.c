#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


/*
Minget copies a regular file from the given source path to the given destination path. 
If the destination path is ommitted, it copies to stdout.

Will need to do:
command line parsing
read a file helper
output to file or stdout
Paths that do not include a leading ‘/’ are processed relative to the root directory

*/

int verbose_flag = 0;
int p_flag = NULL;
int s_flag = NULL;


/*function to use getopts to parse cmd line args*/
int parse_args(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "p:s:v")) != -1) {
        switch (opt) {
            case 'p':
                p_flag = optarg;
                break;
            case 's':
                s_flag = optarg;
                break;
            case 'v':
                verbose_flag = 1;
                break;
            case '?':
                fprintf(stderr, "Unrecognized argument -%c \n", optopt);
                break;
            case ':':
                fprintf(stderr, "Missing argument -%c \n", optopt);
                break;
        }

    }
}
