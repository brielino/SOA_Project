#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

#define PATH "/dev/my-dev"


int main(int argc, char **argv){

    int major;
    int priorita;
    int modal_op;
    int tipo_op;
    int numero_byte;
    int file;
    char *messaggio;
    char comando[1024];

    if(argc < 5){
        printf("Funzionamento: Prog Major Alta-Bassa Bloccante-Non Bloccante Lettura-scrittura\n");
        printf("Alta-Bassa : 1 Alta -- 0 Bassa\n");
        printf("Bloccante-Non Bloccante : 1 Bloccante -- 0 Non Bloccante\n");
        printf("Lettura-Scrittura : 1 Lettura -- 0 Scrittura\n");
        return 0;
    }

    major = strtol(argv[1],NULL,10);
    priorita = strtol(argv[2],NULL,10);
    modal_op = strtol(argv[3],NULL,10);
    tipo_op = strtol(argv[4],NULL,10);

    sprintf(comando,"sudo mknod %s c %d %i\n",PATH,major,0);
    system(comando);
    if(tipo_op == 1){
        //lettura
        printf("E'stata scelta la lettura, per effettural indicare il numero di byte da leggere\n");
        scanf("%d",&numero_byte);
        

        file = open(PATH, O_RDONLY);
        if(file < 0){
            printf("Apertura Device fallita\n");
        }

        if(priorita == 1){
            ioctl(file,10);
        }else{
            ioctl(file,11);
        }
        printf("il valore è %d\n",tipo_op);
        if(modal_op == 1){
            ioctl(file,12);
            printf("BLOccante\n");
        }else{
            ioctl(file,13);
            printf("NOn bloccante\n");
        }

        read(file, messaggio, numero_byte);
        printf("Messaggio %s\n", messaggio);
        if(close(file) < 0){
            printf("File non chiuso con successo\n");
        }else{
            printf("File chiuso con successo\n");
        }
    

    }else{
        printf("E'stata scelta la scrittura, per effetturla indicare la frase da scrivere\n");
        scanf("%s",&messaggio);
        file = open(PATH, O_RDONLY);
        if(file < 0){
            printf("Apertura Device fallita\n");
        }

        if(priorita == 1){
            ioctl(file,10);
        }else{
            ioctl(file,11);
        }
        printf("il valore è %d\n",tipo_op);
        if(modal_op == 1){
            ioctl(file,12);
            printf("BLOccante\n");
        }else{
            ioctl(file,13);
            printf("NOn bloccante\n");
        }
        write(file, messaggio, strlen(messaggio));
        //scrittura
    }
    sprintf(comando,"sudo rm %s \n",PATH);
    system(comando);
    return 0;

}