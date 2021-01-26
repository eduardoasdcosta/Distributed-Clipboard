#include "clipboard.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////
//// This app pastes the entire clipboard n_cycles times
////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    int s = -1, n_cycles = 0, i = 0;
    char dados[2000];
    srand(time(NULL));
    char boom[100];

    if((s = clipboard_connect(PATH)) == -1)
      return 0;

    printf("Insert number of cycles: \n");    
    n_cycles = atoi(fgets(boom, 100, stdin));
    
    while(n_cycles > 0)
    {	
		for(i = 0; i < N_CLIP_REG; i++)
		{
			clipboard_paste(s, i, dados, sizeof(dados));
			printf("Region %d: (%s)\n", i, dados);
		}
		n_cycles--;
    }

    clipboard_close(s);
  
    return 0;
}