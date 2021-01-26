#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <sys/types.h>
#include <unistd.h> 

#define PATH	"./CLIPBOARD_SOCKET" //path to create the file used for local communication by AF_UNIX sockets
#define N_CLIP_REG 10	//number of desired clipboard regions

int clipboard_connect(char * clipboard_dir);	// returns the file descriptor for the new socket on success, -1 on error and prints the correspondent error
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);		// returns number of bytes copied on success, 0 on error and prints the correspondent error
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);		// returns number of bytes copied on success, 0 on error and prints the correspondent error
int clipboard_wait(int clipboard_id, int region, void* buf, size_t count);		// returns number of bytes copied on success, 0 on error and prints the correspondent error
void clipboard_close(int clipboard_id);		// prints error on failure

#endif