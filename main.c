#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
//first we define the macro preprocessor for the copiler to know what to substitute with what
#define CGROUP_ROOT "sys/fs/cgroup"
#define CGROUP_PARENT "c-cgroup"
