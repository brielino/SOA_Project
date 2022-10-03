#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>



int main(int argc, char **argv){

    int major;
    int minor;
    char minor_char[4];
    int priorita;
    int modal_op;
    char yesorno;
    int tipo_op;
    int numero_byte;
    int file;
    char *messaggio;
    char comando[1024];
    char nome_device[100] = "/dev/my-dev";
    if(argc < 2){
        printf("ATTENZIONE!! Per il corretto funzionamento del programma bisogna passare :\n");
        printf("    1° argomento se si vuole effettuare una lettura o una scrittura (Indicare 1 per lettura -- 0 scrittura)\n");
        printf("sudo nome_programma lettura_scrittura\n");

        return 0;
    }

    tipo_op = strtol(argv[1],NULL,10);
    printf("Vuoi mantenere le impostazioni di default?(y/n) ");
    scanf("%s",&yesorno);
    if(strcmp("y",&yesorno) ==0 || strcmp("Y",&yesorno) == 0){
        ioctl(file,7);
    }else{
        printf("Inserire come si vuole eseguire l'operazione:\n    Bloccante 1\n    Non Bloccante 0\n");
        scanf("%d",&modal_op);
        printf("Inserire la priorità desierata:\n    Alta 1\n    Bassa 0\n");
        scanf("%d",&priorita);

        if(priorita == 1){
            ioctl(file,0);
        }else{
            ioctl(file,1);
        }
        if(modal_op == 1){
            ioctl(file,2);
        }else{
            ioctl(file,3);
        }
    }
    if(tipo_op == 1){
        printf("E'stata scelta la lettura, per effettural indicare il numero di byte da leggere\n");
        scanf("%d",&numero_byte);
        printf("Inserire il minor del device (valore compreso da 0 a 128):  \n");
        scanf("%d",&minor);
        sprintf(minor_char,"%d",minor);
        strcat(nome_device,minor_char);
        messaggio = malloc(numero_byte);
        file = open(nome_device, O_RDWR);
        if(file < 0){
            printf("Apertura Device fallita\n");
        }


        read(file, messaggio, numero_byte);
        printf("Messaggio letto da %s\n", messaggio);
        if(close(file) < 0){
            printf("File non chiuso con successo\n");
        }else{
            printf("File chiuso con successo\n");
        }
    

    }else{
        printf("Inserire il minor del device (valore compreso da 0 a 128):  \n");
        scanf("%d",&minor);
        sprintf(minor_char,"%d",minor);
        strcat(nome_device,minor_char);
        file = open(nome_device, O_RDWR);
        if(file < 0){
            printf("Apertura Device fallita\n");
        }

        write(file, "TestProva", 9);
    }
    return 0;

}