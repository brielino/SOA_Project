#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH "/dev/my-dev"


int main(int argc, char **argv){
    char comando[50];
    for(int i =0 ; i < 128; i++){
        sprintf(comando,"sudo rm %s%d \n",PATH,i);
        system(comando);
    }
    return 0;
}