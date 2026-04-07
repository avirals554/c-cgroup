#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>//this is only for showing the error that is caused when a syscall fails , use it only for syscalls
//first we define the macro preprocessor for the copiler to know what to substitute with what
#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_PARENT "c-cgroup"
//this function is gonna write the values that we pass to the path that we pass ,
// keep it mind that in no way it should be called before the create_croup()
//  cause then we are writting on a file that does not exist
// there is gonna be just 3 things in this 1. open file 2. write to the file 3. close the file that is it ..
void write_to_file(const char *path , const char *value ){
    int fd = open(path, O_WRONLY , 0644);
    if(fd==-1){
        printf("the file failed to open ");
    }
    else{
           printf("\nFile opened successfully!\n");
       }

       long  bytesWritten = write(fd, value, strlen(value));
       printf(" %ld bytes written successfully!\n",bytesWritten);

        close(fd);
        printf("the path is closed sucessfully \n");

        return ;
}
//this is a helper function that is gonna find the cgorup of the pid and then is gonna write the values to that cgroup
// this is gonna set a limit to the resources that are at the folder /sys/fs/cgroup/pid
void setLimit(pid_t pid , const char *memory , const char *cpu){
    char memory_path [250];
    char cpu_path [250];
    snprintf(memory_path,sizeof(memory_path),"%s/%s/%d/%s",CGROUP_ROOT,CGROUP_PARENT,pid,"memory.max");
    snprintf(cpu_path,sizeof(cpu_path),"%s/%s/%d/%s",CGROUP_ROOT,CGROUP_PARENT,pid,"cpu.max");
    write_to_file(memory_path,memory);
    write_to_file(cpu_path, cpu);
    return ;

}
//just simple concatination and then doing mkdir that is it

void create_cgroup(pid_t pid){
    char path[256];
    char path_parent[256];
    snprintf(path, sizeof(path),"%s/%s/%d",CGROUP_ROOT,CGROUP_PARENT,pid);
    snprintf(path_parent, sizeof(path_parent),"%s/%s",CGROUP_ROOT,CGROUP_PARENT);
    struct stat st ;
   if (stat(path_parent,&st)==-1){
       mkdir(path_parent,0755);
   }
    if(mkdir(path ,0777) == 0){
        printf("Directory created sucessfully \n");
    }
    else {
        perror("directory could not be created \n");
        return;
    }
    return;
}
int main(int argc,char *argv[]){
    char memory[250] ;
    char cpu[250];
    if (argc<3){
        printf(
            "wrong format , \n either an executable or an program expected ..\n Example : ./cgcontrol run python3 script.py"

        );
        return 0;
    }
    for (int i =0 ; i<argc;i++){
    if (strcmp(argv[i], "--memory") == 0) {
        // next argument is the value
        strcpy(memory,argv[i+1]);
    }
    }
    for (int j =0 ; j<argc;j++){
    if (strcmp(argv[j], "--cpu") == 0) {
        // next argument is the value
        strcpy(cpu ,argv[j+1]);
    }
    }
    int pid=fork();
    create_cgroup(pid);

return 0 ;
}
