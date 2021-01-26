#include "clip_implementation.h"
/*
/////////////////////////////////////////
	Function used to send message info and data byte array to parent clipboard when an app 
	updates the local clipboard.
/////////////////////////////////////////
*/
void ClipSendUp(void* in)
{
    //Variables used to exchange data between processes
	Message m;
	int len_message = sizeof(Message);
	int totalbytes = 0, nbytes = 0, i = 0;
	pthread_t t;
	char** data = (char**) in;
	//copy message data to byte array
	memcpy(&m, data[0], len_message);
	//locks the mutex to guarantee that only one ClipSendUp function is sending info to parent(this ensures that the parent clip receives data in the correct order)
	pthread_mutex_lock(&up_tree_mux); //critical region starts
    //move up the clipboard tree propagating the changed region and its contents but not updating the clipboard! 
	while(totalbytes < len_message) //->this loop ensures that all data is sent even if more than 1 send() call is needed
	{
		//send changed region and the length of the new data
		//the MSG_NOSIGNAL flag allows the program to keep running even if connection with parent clipboard is lost(bypasses broken pipe signal terminating the process)
		if((nbytes = send(s_father, data[0] + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1) 
		{
			perror("Error sending message to parent");
			printf("Warning: Could not propagate changes up the tree!\nChanging to single mode.\n");
			//if connection with parent clipboard was lost, change to single mode, update clipboard and propagate down the tree
			mode = 1;
			ClipSendDown((void*)data);
			return;
		}
		totalbytes += nbytes;
	}
	//reset byte counting variable
	totalbytes = 0;
	while(totalbytes < m.len_data)
	{
		//send the actual data byte array to the parent clipboard
		if((nbytes = send(s_father, data[1] + totalbytes, m.len_data - totalbytes, MSG_NOSIGNAL)) == -1)
		{
			perror("Error sending byte array to parent");
			printf("Warning: Could not propagate byte array up the tree!\nChanging to single mode.\n");
			//if connection with parent clipboard was lost, change to single mode, update clipboard and propagate down the tree
			mode = 1;
			ClipSendDown((void*)data);
			return;
		}
		totalbytes +=nbytes;
	}
	pthread_mutex_unlock(&up_tree_mux); //critical region ends
	
	return;
}
/*
/////////////////////////////////////////
	Thread initially updates the latest connected clipboard (children).
	Thread mainly used to receive data from child clipboards in the tree.
	c is the socket fd that handles the connection between a single child clipboard and the parent clipboard
	If a clipboard has N children connected to it then N ClipThreadRecvDown threads are active
/////////////////////////////////////////
*/
void* ClipThreadRecvDown(void* in) 
{
	//socket handle
	int c = *((int*)in);
	c_connected = 1;
    //Variables used to exchange data between father and son
	Message m;
	int len_message = sizeof(Message);
	int totalbytes = 0, nbytes = 0, i = 0;
	char** data = (char**)malloc(2*sizeof(char*));
	if(data == NULL)
	{
		printf("Failed allocating memory.\n");
		printf("Child with file descriptor (%d) died.\n", c);
		close(c);
		pthread_exit(NULL);
	}
	data[0] = (char*)malloc(len_message);
	if(data[0] == NULL)
	{
		printf("Failed allocating memory.\n");
		printf("Child with file descriptor (%d) died.\n", c);
		close(c);
		free(data);
		pthread_exit(NULL);
	}

	printf("Thread(Clips) with socket handle (%d) is active!\n", c);

    // Replicate local clipboard to recently connected child 
	for(i = 0; i < N_CLIP_REG; i++)
	{
		//lock the clipboard region while data is being copied
		pthread_rwlock_rdlock(&clipboard_locks[i]); //critical region begins
		//retrieve the lenght of the data stored in the i region
		m.len_data = len_messages[i];
		data[1] = (char*) malloc(len_messages[i]);
		if(data[1] == NULL)
		{
			printf("Failed allocating memory.\n");
			printf("Child with file descriptor (%d) died.\n", c);
			close(c);
			free(data[0]);
			free(data);
			pthread_exit(NULL);
		}
		memcpy(data[1], clipboard[i], len_messages[i]);
		pthread_rwlock_unlock(&clipboard_locks[i]); //critical region ends
		//update the message member variables with the right values
		m.region = i;
		m.op = 0; // 0 is copy
		//fill the byte buffer with the message info
		memcpy(data[0], &m, len_message);

		totalbytes = 0;
		//send the message info to child clipboard
		while(totalbytes < len_message)
		{
			if((nbytes = send(c, data[0] + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
			{
				perror("Error sending message to child");
				printf("WARNING: CHILD WITH FILE DESCRIPTOR [%d] WAS NOT UPDATED\n", c);
				break;
			}
			totalbytes += nbytes;
		}

		totalbytes = 0;
		//send the clipboard data to child clipboard
		while(totalbytes < len_messages[i])
		{
			if((nbytes = send(c, data[1] + totalbytes, len_messages[i] - totalbytes, MSG_NOSIGNAL)) == -1)
			{
				perror("Error sending message to child");
				printf("WARNING: CHILD WITH FILE DESCRIPTOR [%d] WAS NOT UPDATED\n", c);
				break;
			}
			totalbytes += nbytes;
		}
		//free data so it can be reallocated in the next cycle
		free(data[1]);
	}

    // Loop used to receive information about an updated region in child clipboards
	while(1)
	{
		totalbytes = 0;
		//receive info about the region that changed and the size of the new data 
		while(totalbytes < len_message)
		{
			if((nbytes = recv(c, data[0] + totalbytes, len_message - totalbytes, 0)) == -1)
			{
				perror("Failed to receive data from Clip");
				printf("Child with file descriptor (%d) died.\n", c);
				//if receive fails we consider the child to be dead, close its descriptor, free memory and exit the thread
				close(c);
				free(data[0]);
				free(data);
				pthread_exit(NULL);
			}
			else if(nbytes == 0) break;
			totalbytes += nbytes;
		}
		if(nbytes == 0) break;//if connection with child is lost leave the while loop to close the file descriptor and exit the thread
		//retrieve the message info from received byte array
		memcpy(&m, data[0], len_message);
		//allocate byte array with enough space receive the new data
		data[1] = malloc(m.len_data);
		if(data[1] == NULL)
		{
			printf("Failed allocating memory.\n");
			printf("Child with file descriptor (%d) died.\n", c);
			close(c);
			free(data[0]);
			free(data);
			pthread_exit(NULL);
		}

		totalbytes = 0;
		//receive the new data that is to be copied to the clipboard region
		while(totalbytes < m.len_data)
		{
			if((nbytes = recv(c, data[1] + totalbytes, m.len_data - totalbytes, 0)) == -1)
			{
				perror("Failed to receive data from Clip");
				printf("Child with file descriptor (%d) died.\n", c);
				close(c);
				free(data[0]);
				free(data[1]);
				free(data);
				pthread_exit(NULL);
			}
			else if(nbytes == 0) break;
			totalbytes += nbytes;
		}
		if(nbytes == 0)
		{
			//if connection is lost free the used byte array and exit the while loop
			free(data[1]);
			break;
		} 

		if(mode)
		{
			//if clipboard is in connected mode send the replication info to parent clipboard 
			ClipSendUp((void*)data);
		}
		else
		{
			//else if in single mode update local clipboard and propagate changes down the tree
			ClipSendDown((void*)data);
		}
		//free byte array so it can be reallocated in the new iteration
		free(data[1]);
	}
	//free used variables, close child file descriptor and exit the thread
	printf("Child with file descriptor (%d) died.\n", c);
	close(c);
	free(data[0]);
	free(data);
	pthread_exit(NULL);
}
/*
/////////////////////////////////////////
	Function used to update local clipboard and propagate the changes down the tree
/////////////////////////////////////////
*/
void ClipSendDown(void* in)
{
    //Variables used to exchange data between processes
	Message m;
	int len_message = sizeof(Message);
	int totalbytes = 0, nbytes = 0;
	int c = 0;
	char** data = (char **) in;
	//list variables
    list_node* aux;
    list_node* next = NULL;
    list_node* prev = NULL;
	//retrieve the message info from the byte array
	memcpy(&m, data[0], len_message);

    //lock the clipboard region while its being updated
    pthread_rwlock_wrlock(&clipboard_locks[m.region]); //critical region start
    //if the current size of the clipboard region is bigger than the new data size, then no allocation is needed,else free memory and reallocate
    if(len_messages[m.region] < m.len_data)
    {
    	free(clipboard[m.region]);
    	clipboard[m.region] = malloc(m.len_data);
    	if(clipboard[m.region] == NULL)
    	{
    		printf("Failed allocating memory.\n");
    		return;
    	}
    }
    //copy new data to clipboard 
    memcpy(clipboard[m.region], data[1], m.len_data);
    //update the byte lenght of the clipboard region
    len_messages[m.region] = m.len_data;
    pthread_rwlock_unlock(&clipboard_locks[m.region]); //critical region ends

    //wake up threads waiting for a change in clipboard[m.region]
    pthread_cond_broadcast(&cond_w[m.region]);

    //lock the list while its being used
    pthread_rwlock_wrlock(&list_lock); //critical region begins
    //set aux to head of list
    aux = l_connected_clips;
    //cycle trough the list and propagate the changed region and new data to all children
    while(aux != NULL)
    {
    	//get child file descriptor from list
    	c = list_get_data(aux);
    	totalbytes = 0;
    	//send the changed region and lenght of data to child
    	while(totalbytes < len_message)
    	{
    		if((nbytes = send(c, data[0] + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
    		{
    			perror("Error sending message to child");
    			printf("WARNING: CHILD WITH FILE DESCRIPTOR [%d] DIED\n", c);
    			//if send fails remove file descriptor from list
    			next = list_get_next(aux);
    			list_remove_element(&l_connected_clips, aux, prev);
    			aux = next;
    			break;
    		}
    		totalbytes += nbytes;
    	}

    	if(nbytes == -1) continue;

    	totalbytes = 0;
    	//send the new data to be copied to clipboard[m.region]
    	while(totalbytes < m.len_data)
    	{
    		if((nbytes = send(c, data[1] + totalbytes, m.len_data - totalbytes, MSG_NOSIGNAL)) == -1)
    		{
    			perror("Error sending message to child");
    			printf("WARNING: CHILD WITH FILE DESCRIPTOR [%d] DIED\n", c);
    			//if send fails remove file descriptor from list
	   			next = list_get_next(aux);
    			list_remove_element(&l_connected_clips, aux, prev);
    			aux = next;
    			break;
    		}
    		totalbytes += nbytes;
    	}

    	if(nbytes == -1) continue;

    	prev = aux;
    	aux = list_get_next(aux);
    }

    pthread_rwlock_unlock(&list_lock); //critical region ends
    
    return;
}
/*
/////////////////////////////////////////
	Thread used to initialize the clipboard(in connected mode) 
	and receive replication info from parent clipboard
/////////////////////////////////////////
*/
void* ClipThreadRecvUp(void* in) 
{
    //Variables used to exchange data between processes
	Message m;
	int len_message = sizeof(Message);
	int totalbytes = 0, nbytes = 0, i = 0;
	pthread_t t;
    //socket handle
	int s = *((int*)in);
	char** data = (char**) malloc(2*sizeof(char*));
	if(data == NULL)
	{
		printf("Failed allocating memory.\n");
		pthread_exit(NULL);
	}

	data[0] = (char*) malloc(len_message);
	if(data[0] == NULL)
	{
		printf("Failed allocating memory.\n");
		free(data);
		pthread_exit(NULL);
	}

    //lock the main thread while the clipboard is being created (no threads can start because the clipboard is still not functional)
	pthread_mutex_lock(&clip_creation_mux); //critical region starts
	clipboard = (void**)malloc(N_CLIP_REG*sizeof(void*));
	if(clipboard == NULL)
	{
		printf("Failed allocating memory.\n");
		//if clipboard was not created, exit the process 
		free(data[0]);
		free(data);
		exit(-1);
	}
	//loop through all clipboard regions and initialize them with info from parent clipboard
	for(i = 0; i < N_CLIP_REG; i++)
	{
		totalbytes = 0;
		//receive data lenght from parent
		while(totalbytes < len_message)
		{	
			if((nbytes = recv(s, data[0] + totalbytes, len_message - totalbytes, 0)) == -1)
			{
				perror("Error receiving message from parent");
				printf("WARNING: CLIPBOARD WAS NOT UPDATED\nChanging to single mode.\n");
				//if receive failed, change to single mode, free memory and exit the thread
				mode = 0;
				free(data[0]);
				free(data);
				free(clipboard);
				pthread_exit(NULL);
			}
			else if(nbytes == 0)
			{
				printf("Lost connection with parent.\nWARNING: CLIPBOARD WAS NOT UPDATED\nChanging to single mode.\n");
				//if connection with parent was lost, change to single mode, free memory and exit the thread
				mode = 0;
				free(data[0]);
				free(data);
				free(clipboard);
				pthread_exit(NULL);
			} 
			totalbytes += nbytes;
		}
		//retrieve message info 
		memcpy(&m, data[0], len_message);
		//update region byte data length
		len_messages[i] = m.len_data;
		//allocate byte array with enough space to receive the new data
		data[1] = malloc(len_messages[i]);
		if(data[1] == NULL)
		{
			printf("Failed allocating memory.\n");
			free(data[0]);
			free(data);
			free(clipboard);
			pthread_exit(NULL);
		}
		//allocate clipboard region
		clipboard[i] = malloc(len_messages[i]);
		if(clipboard[i] == NULL)
		{
			printf("Failed allocating memory.\n");
			//if clipboard allocation failed,free memory and exit the process
			while(i > 0)
			{
				free(clipboard[i - 1]);
				i--;
			}
			free(data[0]);
			free(data[1]);
			free(data);
			free(clipboard);
			exit(-1);
		}

		totalbytes = 0;
		//receive the data to be copied to clipboard[i]
		while(totalbytes < len_messages[i])	
		{	
			if((nbytes = recv(s, data[1] + totalbytes, len_messages[i] - totalbytes, 0)) == -1)
			{
				perror("Error receiving byte array from parent");
				printf("WARNING: CLIPBOARD WAS NOT UPDATED\nChanging to single mode.\n");
				//if receive fails change to single mode, free memory and exit the thread
				//Note: because mode is now 0 the clipboard will be initialized later in the main thread
				mode = 0;
				free(data[0]);
				free(data[1]);
				free(data);
				while(i >= 0)
				{
					free(clipboard[i]);
					i--;
				}
				free(clipboard);
				pthread_exit(NULL);
			}
			else if(nbytes == 0)
			{
				printf("Lost connection with parent.\nWARNING: CLIPBOARD WAS NOT UPDATED\nChanging to single mode.\n");
				//if connection with parent is lost change to single mode, free memory and exit the thread
				mode = 0;
				free(data[0]);
				free(data[1]);
				free(data);
				while(i >= 0)
				{
					free(clipboard[i]);
					i--;
				}
				free(clipboard);
				pthread_exit(NULL);
			}
			else totalbytes += nbytes;
		}
		//copy the contents of byte array to clipboard region
		memcpy(clipboard[i], data[1], len_messages[i]);
		free(data[1]);
	}

	//wake up main thread
	pthread_cond_signal(&cond_v);
	pthread_mutex_unlock(&clip_creation_mux);//critical region ends

	//receive replication info from parent clipboard
	while(1)
	{
		totalbytes = 0;
		//receive changed region and length of the new data
		while(totalbytes < len_message)
		{
			if((nbytes = recv(s, data[0] + totalbytes, len_message - totalbytes, 0)) == -1)
			{
				perror("Error receiving message from parent");
				printf("Changing to single mode.\n");
				//if receive fails change to single mode, free memory and exit thread
				mode = 0;
				free(data[0]);
				free(data);
				pthread_exit(NULL);
			}
			else if(nbytes == 0)
			{
				printf("Lost connection with parent.\nChanging to single mode.\n");
				//if connection with parent was lost change to single mode, free memory and exit thread
				mode = 0;
				free(data[0]);
				free(data);
				pthread_exit(NULL);
			}
			totalbytes += nbytes;
		}

		//retrieve the message contents
		memcpy(&m, data[0], len_message);
		//allocate byte array with enough space for the new data
		data[1] = (char*) malloc(m.len_data);
		if(data[1] == NULL)
		{
			printf("Failed allocating memory.\n");
			free(data[0]);
			free(data);
			pthread_exit(NULL);
		}

		totalbytes = 0;
		//receive the new data to be copied to clipboard[m.region]
		while(totalbytes < m.len_data)
		{
			if((nbytes = recv(s, data[1] + totalbytes, m.len_data - totalbytes, 0)) == -1)
			{
				perror("Error receiving byte array from parent");
				printf("Changing to single mode.\n");
				//if receive fails change to single mode, free memory and exit thread
				mode = 0;
				free(data[0]);
				free(data[1]);
				free(data);
				pthread_exit(NULL);
			}
			else if(nbytes == 0)
			{
				printf("Lost connection with parent.\nChanging to single mode.\n");
				//if connection with parent was lost change to single mode, free memory and exit thread
				mode = 0;
				free(data[0]);
				free(data[1]);
				free(data);
				pthread_exit(NULL);
			}
			totalbytes += nbytes;
		}
		//update the clipboard and propagate changes down the tree
		ClipSendDown((void*)data);
		//free byte array so it can be allocated in the next iteration
    	free(data[1]);
	}
	//free used variables and exit thread
	free(data[0]);
	free(data);
	pthread_exit(NULL);
}