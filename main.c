#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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

void create_cgroup(pid_t pid){
    char path[256];
    char path_parent[256];
    snprintf(path, sizeof(path),"%s/%s/%d",CGROUP_ROOT,CGROUP_PARENT,pid);
    snprintf(path_parent, sizeof(path_parent),"%s/%s",CGROUP_ROOT,CGROUP_PARENT);
    if(mkdir(path_parent, 0777)==0){
        printf("the parent has been created \n");
    }
    else{
        printf("parent failed \n");
        return;
    }
    if(mkdir(path ,0777) == 0){
        printf("Directory created sucessfully \n");
    }
    else {
        printf("directory could not be created \n");
        return;
    }
    return;
}
int main(){

return 0 ;
}
