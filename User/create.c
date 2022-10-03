#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH "/dev/my-dev"



int main(int argc, char **argv){
    char comando[50];
    if(argc < 2){
        printf("Inserire il major number\n");
        printf("sudo nome_programma major\n");
    }
    major = strtol(argv[1],NULL,10);
    for(int i =0 ; i < 128; i++){
        sprintf(comando,"mknod %s c %d %i\n",PATH,major,i);
        system(comando);
    }
    return 0;
}