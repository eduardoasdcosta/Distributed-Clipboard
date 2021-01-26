#include "clipboard.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// This app is the normal functional app, allowing to choose the operations and the clipboard regions
//// It makes n_cycles operations
////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    int s = -1, mode = 0, region = 0, n_cycles = 0, nbytes = 0;
    char dados[2000];
    char* dados2;
    srand(time(NULL));
    char boom[100];

    if((s = clipboard_connect(PATH)) == -1)
      return 0;

  	printf("Insert number of cycles: \n");	
	n_cycles = atoi(fgets(boom, 100, stdin));
    
    while(n_cycles > 0)
    {	
		printf("Copy/Paste/Wait ( 0 / 1 / 2 )\n");	
		mode = atoi(fgets(boom, 3, stdin));

		if(mode == 0)
		{
		    printf("Please enter region: ");
		    region = atoi(fgets(boom, 3, stdin));
		    printf("Please enter data: ");
		    fgets(dados, 2000, stdin);
		    dados2 = (char*)malloc(strlen(dados) + 1);
		    if(dados2 == NULL)
		    	return -1;
		    memcpy(dados2, dados, strlen(dados));
		    dados2[strlen(dados)] = '\0';
		    
		    if((nbytes = clipboard_copy(s, region, dados2, strlen(dados2) + 1) == 0))
		      	break;
		  	free(dados2);
		}
		else if(mode == 1)
		{
		    printf("Please enter region: ");
		    region = atoi(fgets(boom, 3, stdin));
		    if((nbytes = clipboard_paste(s, region, dados, sizeof(dados))) == 0)
		      break;
		    printf("App received (%s) with %d bytes.\n", dados, nbytes);
		}	
		else if(mode == 2)
		{
			printf("Please enter region: ");
		    region = atoi(fgets(boom, 3, stdin));
		    if((nbytes = clipboard_wait(s, region, dados, sizeof(dados))) == 0)
		      break;
		    printf("App received (%s) with %d bytes.\n", dados, nbytes);
		}	
		else printf("Invalid argument!\n");
		n_cycles--;
    }

    clipboard_close(s);
  
    return 0;
}
