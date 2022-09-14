#include <string.h>

#define MINORS 128
#define DEVICE_NAME "my_device"


int main(void){

    int i;
    for(i=0;i<MINORS;i++){
        char name_device[10] = DEVICE_NAME;
        strcat(name_device,i+ '0');
        printf("%s\n",name_device);

    }
    return 0;
}
