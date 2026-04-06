# c-cgroupcontrol

a mini cgroup controller i built from scratch to understand how linux actually manages process resources. 

---

## what even is this project

so basically this is a C program that wraps any process you want to run and automatically:
- creates a cgroup for it
- sets memory and CPU limits on it
- puts the process inside that cgroup
- makes sure it gets killed if it goes over the limit

you run it like this:
```bash
./cgcontrol run python3 script.py
```

and behind the scenes it does everything that docker, kubernetes, and systemd do — just way simpler and you can actually read the code.

---

## Motivation:



> "when you run a program on linux, what do you think the kernel is actually tracking about that process?"

and i had to think. and be wrong. and think again.

turns out that's way better than reading docs.

if you want to learn this stuff too — don't read. experiment. open a terminal and run the commands yourself. the confusion IS the learning.

---

## everything i actually learned

### 1. what the kernel tracks about a process

every running process is stored inside the kernel as a `task_struct` — a big C struct that holds:
- its PID (process ID)
- its memory maps
- its open files
- its parent process
- its CPU state
- and most importantly for us — **which cgroup it belongs to**

you can see a lot of this by running:
```bash
cat /proc/<pid>/status
```

### 2. /proc is not a real folder

`/proc` is a virtual filesystem. nothing in there exists on your hard drive. when you `cat /proc/699/status`, the kernel literally runs code and pretends the output is a file.

same with `/sys/fs/cgroup`. these are all fake files that the kernel uses to let you talk to it using normal file tools like `echo` and `cat`.

this is genius design — you don't need to learn any special API. if you can write to a file, you can control the kernel.

### 3. what a cgroup actually is

a cgroup is just a **folder** inside `/sys/fs/cgroup/`.

that's literally it. creating a cgroup is just:
```bash
mkdir /sys/fs/cgroup/whatever
```

the kernel notices the mkdir and automatically fills that folder with interface files like `memory.max`, `cpu.max`, `cgroup.procs` etc.

a cgroup is more like a **label** than a jail. you can move processes between cgroups freely. it just defines rules — not walls.

### 4. how limits actually work

the files inside a cgroup folder are how you set limits:

```bash
# set memory limit to 50MB
echo 52428800 > /sys/fs/cgroup/myfirst/memory.max

# set CPU limit to 20% of one core
echo "200000 1000000" > /sys/fs/cgroup/myfirst/cpu.max

# put a process inside the cgroup
echo <pid> > /sys/fs/cgroup/myfirst/cgroup.procs
```

when the process tries to use more than 50MB, the kernel checks its cgroup, sees the limit, and sends SIGKILL. that's it.

### 5. the memory files explained

there are 4 memory files and they're like graduated responses:

| file | what it does |
|------|-------------|
| `memory.min` | guaranteed — kernel never reclaims below this |
| `memory.low` | protection — kernel tries not to reclaim below this |
| `memory.high` | soft limit — kernel starts reclaiming aggressively |
| `memory.max` | hard limit — process gets killed if it exceeds this |

also always set `memory.swap.max = 0` alongside `memory.max`. otherwise when you hit the limit the process just gets swapped to disk instead of killed — which causes weird latency instead of a clean death.

### 6. the cgroup hierarchy

cgroups are a tree. every cgroup has a parent. limits cascade down — the effective limit is always the minimum along the path from process to root.

```
root (8GB)
  └── myapp (4GB)      ← effective limit is 4GB
        └── worker (6GB)  ← effective limit is still 4GB, not 6GB
```

on my google cloud shell the hierarchy looked like this:
```
/sys/fs/cgroup/
└── k8s.io/                    ← kubernetes manages all containers
    └── 3d8f6f63ac93.../       ← MY container (that hash is my container ID)
        └── (my shell lives here)
```

google set a 15GB limit on my container from outside, before i even logged in. i can't change it. that's the point.

### 7. what i found on cloud shell

ran this:
```bash
cat /proc/$$/cgroup
# 0::/k8s.io/3d8f6f63ac932eaea905aceb8f4549b019a92f4917365b6eb0011af11d809476
```

that hash is my container ID — same format as `docker ps`. kubernetes literally did a `mkdir` of that hash when google cloud shell started my session.

also ran:
```bash
ls /sys/fs/cgroup/k8s.io/
```

and saw other hashes — other people's containers on the same physical machine. i could read their `memory.current` (how much RAM they're using). cgroups give you **no privacy** — just resource limits.

### 8. cgroups vs namespaces

this is the big one i kept confusing:

- **cgroups** = how much of a resource can this group use
- **namespaces** = what can this process even see

docker uses both together:
- cgroups limit the resources
- namespaces hide everything (other processes, filesystem, network)

without namespaces, containers can see each other. without cgroups, they can eat each other's resources.

`chroot` was the old way to fake a filesystem — but a chrooted process can still see all other processes via `/proc`, use the host network, etc. namespaces fixed that properly.

### 9. fork() and exec()

this is how you launch a program from C:

```
your program
    │
    fork()  ← creates an exact copy of your process
    │
    ├── parent → waits, monitors
    │
    └── child (copy of your program, same code, new PID)
            │
            exec("python3 script.py")  ← replaces child with python3
            │
            child is now python3, but keeps same PID
```

the key insight for our controller: the child writes its own PID to `cgroup.procs` **after** `fork()` but **before** `exec()`. that way python3 starts its life already inside the cgroup.

### 10. virtual filesystems

`/proc` and `/sys/fs/cgroup` are not on your hard drive. they're `procfs` and `cgroupfs` — filesystem types that exist only in kernel memory, mounted at boot.

writing `52428800` to `memory.max` doesn't write to disk. it calls kernel code that sets a value in the kernel's internal memory accounting data structure.

the kernel exposes everything as files because then every tool that works with files (echo, cat, python's open(), shell scripts) works with kernel internals for free. composability.

---

## what i've built so far

### the includes
```c
#include <stdio.h>      // printf
#include <stdlib.h>     // malloc, exit
#include <string.h>     // strlen, strcpy
#include <unistd.h>     // fork, exec, getpid — core unix syscalls
#include <fcntl.h>      // open(), O_WRONLY flags
#include <sys/stat.h>   // mkdir
#include <sys/types.h>  // pid_t and other type definitions
#include <sys/wait.h>   // waitpid
```

### the defines
```c
#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_PARENT "c-cgroup"
```

`#define` is a preprocessor directive — before compilation even starts, every occurrence of `CGROUP_ROOT` gets replaced with `"/sys/fs/cgroup"`. it's not a variable. no memory allocated. just text substitution.

### write_to_file()
helper function. opens a file, writes a value, closes it. used to set limits by writing to cgroup files.

uses the low level syscall API:
- `open()` returns an int (file descriptor — basically a ticket number)
- `write(fd, value, strlen(value))` writes to that ticket
- `close(fd)` gives the ticket back

not `fopen`/`fclose` — those are higher level C library wrappers. we're talking directly to the kernel.

### create_cgroup()
takes a PID, creates two directories:
1. `/sys/fs/cgroup/c-cgroup/` — the parent (created once)
2. `/sys/fs/cgroup/c-cgroup/<pid>/` — per-process cgroup

uses `snprintf` to build the path string:
```c
char path[256];  // buffer — just an array of 256 chars on the stack
snprintf(path, sizeof(path), "%s/%s/%d", CGROUP_ROOT, CGROUP_PARENT, pid);
```

### what's next
- `set_limits()` — write memory.max and cpu.max
- `main()` — parse arguments, fork, exec, assign PID to cgroup
- cleanup on exit — rmdir the cgroup after process dies

---

## the design of the cgroup structure

```
/sys/fs/cgroup/
└── c-cgroup/              ← parent: all processes managed by this controller
    ├── 1234/              ← per-process cgroup (PID as folder name)
    ├── 1235/
    └── 1236/
```

PIDs are unique — no two live processes share a PID. so using PID as folder name guarantees uniqueness. and if you want to nuke everything this controller ever made, just `rm -rf /sys/fs/cgroup/c-cgroup/`.

this is the same design as kubernetes — `k8s.io/` is the parent, each container hash is a child.

---

## tools i use

```bash
man 2 <syscall>    # read the manual for any kernel syscall
man 3 <function>   # read the manual for any C library function
gcc -Wall -Wextra  # compile with all warnings on — the compiler is your friend
cat /proc/$$/cgroup  # see which cgroup your current shell is in
```

---

## how to run this (on linux only — cgroups don't exist on mac)

```bash
git clone https://github.com/yourusername/c-cgroupcontrol
cd c-cgroupcontrol
gcc -Wall -Wextra -o cgcontrol cgcontrol.c
sudo ./cgcontrol run python3 script.py
```

needs `sudo` because writing to `/sys/fs/cgroup` requires root. same reason google cloud shell needs it.

---

## random things that blew my mind

- docker, kubernetes, systemd are all just doing `mkdir` and `echo` into `/sys/fs/cgroup`. that's literally it.
- i was already inside a cgroup before i even started this project. my entire cloud shell session lives in one.
- strings in C are just arrays of chars with a `\0` at the end. there is no string type. a `char *` is just a pointer to where the array starts in memory.
- the kernel is always running. every malloc, every CPU tick, every syscall goes through kernel code that checks: what cgroup does this process belong to? charge it.
- you can see other people's container memory usage on cloud shell. cgroups are not privacy — they're just resource accounting.

---

*built while learning. probably has bugs. clone it in a container or on cloud if you like your pc , wont work on mac and of cource windows.*
