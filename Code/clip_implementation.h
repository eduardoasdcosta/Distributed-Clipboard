#ifndef CLIP_IMPLEMENTATION_H
#define CLIP_IMPLEMENTATION_H

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>           
#include <pthread.h>

#include "clipboard.h"
#include "list.h"

typedef struct Data
{
    int region;		// region of the clipboard 
    int op;			// 0 is copy, 1 is paste, 2 is wait
    int len_data;	// size of the data byte array
}Message;

//Local Regions shared by all threads
void** clipboard;
//Current lenght of the data byte array present in the different regions
int len_messages[N_CLIP_REG];
//Flag that informs the main thread if the newly created thread has received the socket id(for Apps)
int a_connected;
//Flag that informs the main thread if the newly created thread has received the socket id(for Clips)
int c_connected;

//Read-Write Lock variable for clipboard regions
pthread_rwlock_t clipboard_locks[N_CLIP_REG];
//Read-Write Lock variable for connected clipboards list (children)
pthread_rwlock_t list_lock;
//mutex used to control clipboard initialization
pthread_mutex_t clip_creation_mux;
//conditional variable for controlling clipboard initialization
pthread_cond_t cond_v;
//mutex used to wait for changes in a clipboard region
pthread_mutex_t wait_muxs[N_CLIP_REG];
//conditional variable for controlling wait function
pthread_cond_t cond_w[N_CLIP_REG];
//mutex used to control comunication up the clipboard tree
pthread_mutex_t up_tree_mux;

//////////////////////////////////////////////////////////////////
//descriptor for the list of connected clipboards (children)
list_node* l_connected_clips;
//Socket to communicate with parent clipboard
int s_father;
// Variable used to check connected or single mode
int mode; // 0 is single, 1 is connected

void* clipAccept(void* in);	// thread that is continuously looking for clipboards to accept
void* Appthread(void* in);	// thread that is continuously looking for local client apps to accept
void ClipSendUp(void* in);	// function that sends Message to the parent, containing a region update information
void* ClipThreadRecvDown(void* in);	// thread that updates initially the children clipboard and continuously looks for Message sent by connected clipboards
void  ClipSendDown(void* in);		// function that updates the clipboard and sends Message to the connected clipboards, so they can update and also send to clipboards connected to them
void* ClipThreadRecvUp(void* in);	// thread that is continuously looking for Message sent by the father clipboard

#endif