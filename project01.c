#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[]){
    printf(1, "My student id is 2018015932\n");
    int pid = getpid();
    int gpid = getgpid();

    printf(1, "My pid is %d\n", pid);
    printf(1, "My gpid is %d\n", gpid);
    exit();
}
