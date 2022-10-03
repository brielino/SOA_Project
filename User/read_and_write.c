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
        printf("ATTENZIONE!! Per il corretto funzionamento del programma bisogna passare :\n    1° argomento Major Number\n");
        printf("    2° argomento se si vuole effettuare una lettura o una scrittura (Indicare 1 per lettura -- 0 scrittura)\n");
        printf("    3° argomento se si vuole un operazione bloccante o no (1 bloccante -- 0 non bloccante)\n");
        printf("    4° argomento il tipo di priorità che si vuole adottare (1 alta prirità -- 0 bassa priorità)\n");
        printf("Utilizzare il prefisso sudo\n");
        printf("sudo nome_programma major lettura_scrittura bloc priorità\n");

        return 0;
    }

    major = strtol(argv[1],NULL,10);
    tipo_op = strtol(argv[2],NULL,10);
    modal_op = strtol(argv[3],NULL,10);
    priorita = strtol(argv[4],NULL,10);

    sprintf(comando,"mknod %s c %d %i\n",PATH,major,0);
    system(comando);
    if(tipo_op == 1){
        printf("E'stata scelta la lettura, per effettural indicare il numero di byte da leggere\n");
        scanf("%d",&numero_byte);
        
        messaggio = malloc(numero_byte);
        file = open(PATH, O_RDWR);
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
        }else{
            ioctl(file,13);

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
        //scanf("%s",&messaggio);
        file = open(PATH, O_RDWR);
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
        write(file, "ciao", 4);
        //scrittura
    }
    sprintf(comando,"sudo rm %s \n",PATH);
    system(comando);
    return 0;

}