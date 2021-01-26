#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "clipboard.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
//// This app connects and disconnects n_cycles apps, each one making 1 random copy in a random region
////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    int s = -1, n_cycles = 0;
    char dados[2000];
    srand(time(NULL));
    char boom[100];

    printf("Insert number of cycles: \n");  
    n_cycles = atoi(fgets(boom, 100, stdin));

    while(n_cycles > 0)
    {	
        if((s = clipboard_connect(PATH)) == -1)
            return 0;
		sprintf(dados, "%c", 48 + rand()%70);
		clipboard_copy(s, rand()%N_CLIP_REG, dados, sizeof(dados));
		usleep(1);
        clipboard_close(s);
		n_cycles--;
    }
  
    return 0;
}
