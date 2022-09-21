#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <linux/slab.h>
int main ()
{
   char *dest;
   char *src  = "newstring";

   printf("Before memmove dest = %s, src = %s\n", dest, src);
   memmove(&dest, &src, 8);
   memmove(&dest, &dest + 8, 0);
   memset(&dest +8, 0, 8);
   dest = krealloc(&dest, 0);
   printf("After memmove dest = %s\n", dest);

   return(0);
}