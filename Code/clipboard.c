#include "clip_implementation.h"

//////////
// INPUT ARGUMENTS: none (if single mode) 	| | | 	-c IP FATHER_PORT (if connected mode)
//////////
// Example: 		./clipboard 		  	| | | 	./clipboard -c 127.0.0.1 8000
//////////

int main(int argc, char** argv)
{
	//variable used at cycles
	int i = 0;
    //app Socket variables
	int s = 0, c = 0;
	struct sockaddr_un addr, c_addr;
	struct sockaddr_in addr2;
	socklen_t c_addr_size, addr2_addr_size;
    //thread that acceps client apps
	pthread_t app_thread;
    //thread that accepts clipboards
	pthread_t clip_thread;
    //thread that connects to parent clipboard
	pthread_t up_thread;
	//time seed used to generate a random port
	srand(time(NULL));

    //checking argument count
	if(argc != 1 && argc != 4)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}
	else if(argc == 4 && strcmp(argv[1], "-c") == 0)
	{
		mode = 1; // connected mode
	}

	//initialize global variables
	a_connected = 1;
	c_connected = 1;
	s_father = 0;
	l_connected_clips = list_init();

	/////////////////////////////////////////////////////////////////
	//initializes mutexs, mutex conditions and read/write locks
	if(pthread_mutex_init(&clip_creation_mux, NULL) != 0)
	{	
		perror("Failed to initialize clip creation mutex");
		return -1;
	}
	if(pthread_cond_init(&cond_v, NULL) != 0)
	{	
		perror("Failed to initialize clip creation cond variable");
		return -1;
	}
	if(pthread_mutex_init(&up_tree_mux, NULL) != 0)
	{	
		perror("Failed to initialize up tree mutex");
		return -1;
	}

	//init rwlock and wait mutex in all the clipboard regions
	for(i = 0; i < N_CLIP_REG; i++)
	{
		if(pthread_rwlock_init(&clipboard_locks[i], NULL) != 0)
		{	
			perror("Failed to initialize rwlock");
			return -1;
		}
		if(pthread_mutex_init(&wait_muxs[i], NULL) != 0)
		{	
			perror("Failed to initialize wait mutex");
			return -1;
		}
		if(pthread_cond_init(&cond_w[i], NULL) != 0)
		{	
			perror("Failed to initialize wait cond variable");
			return -1;
		}
	}
	if(pthread_rwlock_init(&list_lock, NULL) != 0)
	{	
		perror("Failed to initialize rwlock");
		return -1;
	}

	/////////////////////////////////////////////////////////////////
	//Socket for local communication with client app

	//creates the local socket
	if((s = socket(AF_UNIX, SOCK_STREAM, 0))==-1)
	{
		perror("Failed to create local socket");
		return -1;
	}
	//sets all socket memory to 0
	memset(&addr, 0, sizeof(struct sockaddr_un));
	//specifies the socket domain: AF_UNIX, since communication via this socket will be processed in the same machine
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, PATH, sizeof(addr.sun_path) - 1);
	//unlinks path in case it has been linked before
	unlink(PATH);
	//binds the local socket
	if(bind(s, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))== -1)
	{
		perror("Failed to bind local socket");
		return -1;
	}	
	//puts the local socket on listen mode
	if(listen(s, 0) == -1)
	{
		perror("Failed to put local socket on listen mode");
		return -1;
	}

    /////////////////////////////////////////////////////////////////
    //INET Socket used to connect to father clipboard (the one it connected to)

    if(mode) // if in connected mode
    {
    	//creates the socket
    	if((s_father = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    	{
    		perror("Failed to create INET socket");
    		return -1;
    	}
    	//sets all socket memory to 0
    	memset(&addr2, 0, sizeof(struct sockaddr_in));
		//specifies the socket domain: AF_INET, since communication via this socket will be processed in different machines
    	addr2.sin_family = AF_INET;
    	//port of father clipboard
    	addr2.sin_port = htons(atoi(argv[3]));
    	//ip of father clipboard
    	inet_aton(argv[2], &addr2.sin_addr);	

    	addr2_addr_size = sizeof(struct sockaddr);
    	//connects with father clipboard
    	if(connect(s_father, (struct sockaddr *) &addr2, addr2_addr_size) == -1)
    	{
    		perror("Failed to connect to remote clipboard");
    		return -1;
    	}
		// Creates thread that will receive information from the father clipboard
    	if(pthread_create(&up_thread, NULL, ClipThreadRecvUp, (void*)&s_father) != 0)
    	{ 
    		printf("Error creating thread for communication with the father clipboard.\nWARNING: CLIPBOARD WILL NOT COMMUNICATE WITH FATHER CLIPBOARD.\n");
    		mode = 0;	    		
    	}
    	else //if the thread is successfully created
    	{
    		//detaches the thread, so that it doesnt enter in zombie mode when connection with father clipboard is lost
    		if(pthread_detach(up_thread) != 0) printf("Error detaching thread.\nWARNING: CLIPBOARD AT RISK OF RUNNING OUT OF THREADS\n");
	    	//wait for clipboard to be created
    		pthread_mutex_lock(&clip_creation_mux);
    		pthread_cond_wait(&cond_v, &clip_creation_mux);
    		pthread_mutex_unlock(&clip_creation_mux);
    		//clipboard was successfully created
    	}
    }

    if(!mode) // if single mode
    {
    	// allocs memory for the clipboard's region pointers
    	clipboard = (void**)malloc(N_CLIP_REG*sizeof(void*));
    	if(clipboard == NULL)
    	{
    		printf("Failed allocating memory.\n");
    		return -1;
    	}

    	for(i = 0; i < N_CLIP_REG; i++)
    	{
    		//allocs 1 byte of memory for each region of the clipboard
    		clipboard[i] = malloc(sizeof(char));
    		if(clipboard[i] == NULL)
    		{
    			printf("Failed allocating memory.\n");
    			while(i > 0)
    			{
    				free(clipboard[i - 1]);
    				i--;
    			}
    			free(clipboard);
    			return -1;
    		}
    		//sets the clipboard regions memory to 0
    		memset(clipboard[i], '\0', 1);
    		//updated the current size of each region
    		len_messages[i] = 1;
    	}
    }

    /////////////////////////////////////////////////////////////////
    //creates thread that accepts clips
    if(pthread_create(&clip_thread, NULL, clipAccept, NULL) != 0) printf("Error creating thread for accepting clips.\nWARNING: CLIPBOARD WILL NOT ACCEPT OTHER CLIPBOARD CONNECTIONS.\n");
    else
    {
    	//detaches the thread, so that it doesnt enter in zombie mode in case something goes wrong and the thread exits
    	if(pthread_detach(clip_thread) != 0) printf("Error detaching thread.\nWARNING: CLIPBOARD AT RISK OF RUNNING OUT OF THREADS\n");
    }

    /////////////////////////////////////////////////////////////////
    //accept loop for client apps
    c_addr_size = sizeof(struct sockaddr);
    
    while(1)
    {
    	//waits until the previously created app thread has it's own socket ID
    	if(a_connected)
    	{
    		//accepts client app connections
    		c = accept(s, (struct sockaddr *) &c_addr, &c_addr_size);
    		if(c == -1) printf("Failed to accept a app.\n");
    		else // if it successfully accepts to the client app
    		{
    			a_connected = 0; // no more app client connections are accepted until a_connected == 1 
    			//creates the thread that communicates with the client app
    			if(pthread_create(&app_thread, NULL, Appthread, (void*)&c) != 0) 
				{
					a_connected = 1; //ready to accept more client connections, since the thread creation failed and the current socket ID (c) is no longer needed
					printf("Error creating thread for apps.\nWARNING: CLIPBOARD DID NOT ACCEPT APP.\n");
				}
    			else
				{
					//detaches the thread, so that it doesnt enter in zombie mode in case the client app disconnects
					if(pthread_detach(app_thread) != 0) printf("Error detaching thread.\nWARNING: CLIPBOARD AT RISK OF RUNNING OUT OF THREADS\n");
					printf("Accepted app with file descriptor (%d).\n", c);
					
				}
    		}
    	}	
    }
    
    /////////////////////////////////////////////////////////////////
    //free regions, destroy mutex, rwlock, clipboard list and unlink path
    l_connected_clips = list_destroy(l_connected_clips);
    for(i = 0; i < N_CLIP_REG; i++)
    {
    	free(clipboard[i]);
    }
    free(clipboard);
    for(i = 0; i < N_CLIP_REG; i++)
    {
    	pthread_rwlock_destroy(&clipboard_locks[i]);
		pthread_mutex_destroy(&wait_muxs[i]);
		pthread_cond_destroy(&cond_w[i]);
    }
    pthread_rwlock_destroy(&list_lock);
    pthread_mutex_destroy(&clip_creation_mux);
    pthread_mutex_destroy(&up_tree_mux);
    pthread_cond_destroy(&cond_v);
    unlink(PATH);

    return 0;
}

void* clipAccept(void* in)
{
	//socket variables
	int s = 0, c = 0;
	struct sockaddr_in addr, c_addr; 
	socklen_t c_addr_size;
    //thread for accepting Clips
	pthread_t clip_thread;
    //////////////////////////////////////////////////////////////////
    //INET Socket used to accept connection from other clipboards
    //creates the socket
	if((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Error creating INET socket for communication with clipboard");
		printf("WARNING: CLIPBOARD WILL NOT ACCEPT OTHER CLIPBOARD CONNECTIONS. CONSIDER RESTARTING CLIPBOARD.\n");
		pthread_exit(NULL);
	}	
	//sets all socket memory to 0
	memset(&addr, 0, sizeof(struct sockaddr_in));
	//specifies the socket domain: AF_INET, since communication via this socket will be processed in different machines
	addr.sin_family = AF_INET;
	//sets the port of the current clipboard to a random number between 8000 and 8100, since this are the available ports at SCDEEC
	int port = rand()%101 + 8000;
	printf("Port is %d\n", port); 	// the port is printed so that other users can connect to this clipboard
	addr.sin_port = htons(port);	// port of the clipboard
	addr.sin_addr.s_addr = INADDR_ANY; // socket is bound to all local interfaces
	//binds the socket
	if(bind(s, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1)
	{
		perror("Failed to bind server side INET socket");
		printf("WARNING: CLIPBOARD WILL NOT ACCEPT OTHER CLIPBOARD CONNECTIONS.\n");
		pthread_exit(NULL);
	}	
	//puts the created socket on listen mode
	if(listen(s, 0) == -1)
	{
		perror("Failed to put INET socket on listen mode");
		printf("WARNING: CLIPBOARD WILL NOT ACCEPT OTHER CLIPBOARD CONNECTIONS.\n");
		pthread_exit(NULL);
	}

	c_addr_size = sizeof(struct sockaddr);

    //////////////////////////////////////////////////////////////////
    //accept loop for clipboard connections
	while(1)
	{
		//waits until the previously created clip thread has it's own socket ID
		if(c_connected)
		{
			//accepts clipboard connections
			c = accept(s, (struct sockaddr *) &c_addr, &c_addr_size);
			if(c == -1) printf("Failed to accept a clip.\n");
			else //if successfully accepted the clipboard connection
			{
				c_connected = 0; //no more clipboard connections are accepted until c_connected == 1 
				if(pthread_create(&clip_thread, NULL, ClipThreadRecvDown, (void*)&c) != 0) 
				{
					c_connected = 1; //ready to accept more clipboard connections, since the thread creation failed and the current socket ID (c) is no longer needed
					printf("Error creating thread for clips.\nWARNING: CLIPBOARD DID NOT ACCEPT CLIP.\n");
				}
				else
				{
					//detaches the thread, so that it doesnt enter in zombie mode in case the clipboard disconnects
					if(pthread_detach(clip_thread) != 0) printf("Error detaching thread.\nWARNING: CLIPBOARD AT RISK OF RUNNING OUT OF THREADS\n");
					printf("Accepted clipboard with file descriptor (%d)\n", c);
					pthread_rwlock_wrlock(&list_lock); //list critical region begins
					l_connected_clips = list_add_element(l_connected_clips, c);	// adds the new connected clipboard to the list of connected clipboards (children)
					pthread_rwlock_unlock(&list_lock); //list critical region ends
				}
			}
		}	
	}
	pthread_exit(NULL);
}

void* Appthread(void* in)
{
	//sets the socket handle
	int c = *((int*)in);
	a_connected = 1; // app thread' socket handle can change at main function, since the thread has it's value now
    //Variables used to exchange data between processes
	Message m;
	int len_data = 0, len_message = sizeof(Message);
	int nbytes = 0, totalbytes = 0;
	int i = 0;
	//allocates memory for 2 pointers, one for the data byte array and the other for Message, containing information of the data byte array
	char** data = (char**) malloc(2*sizeof(char*));
	if(data == NULL)
	{
		printf("Failed allocating memory.\n");
		close(c);
		pthread_exit(NULL);
	}
	//allocates memory for Message
	data[0] = (char*) malloc(len_message);
	if(data[0] == NULL)
	{
		printf("Failed allocating memory.\n");
		free(data);
		close(c);
		pthread_exit(NULL);
	}

	printf("Thread(Apps) with socket handle (%d) is active!\n", c);
    
    //main loop to interact with App
	while(1)
	{
		//receive Message from App
		totalbytes = 0;
		while(totalbytes < len_message)
		{
			if((nbytes = recv(c, data[0] + totalbytes, len_message - totalbytes, 0)) == -1)
			{
				perror("Failed to receive data from App");
				free(data[0]);
				free(data);
				close(c);
				pthread_exit(NULL);
			}
			else if(nbytes == 0)
			{
				printf("App closed.\n");
				free(data[0]);
				free(data);
				close(c);
				pthread_exit(NULL);
			} 
			totalbytes += nbytes;
		}
		//copies the content to Message so they are readable
		memcpy(&m, data[0], len_message);

		//if operation is copy
		if(m.op == 0)
		{
			totalbytes = 0;
			//allocs the memory for the data byte array
			data[1] = (char*) malloc(m.len_data);
			if(data[1] == NULL)
			{
				printf("Failed allocating memory.\n");
				free(data[0]);
				free(data);
				close(c);
				pthread_exit(NULL);
			}
			//receives data byte array
			while(totalbytes < m.len_data)
			{
				if((nbytes = recv(c, data[1] + totalbytes, m.len_data - totalbytes, 0)) == -1)
				{
					perror("Failed receiving byte array from app");
					free(data[0]);
					free(data[1]);
					free(data);
					close(c);
					pthread_exit(NULL);
				}
				else if(nbytes == 0) // app closed
				{
					printf("App closed.\n");
					free(data[0]);
					free(data[1]);
					free(data);
					close(c);
					pthread_exit(NULL);
				} 
				totalbytes += nbytes;
			}
	
			if(mode)
			{
				ClipSendUp((void*)data); 	// connected mode, so data will be sent up the tree (father)
			}
			else
			{
				ClipSendDown((void*)data);	// single mode, so data will be sent down the tree (children)
			}	 
			//frees data byte array
			free(data[1]);   	
		}

		//if operation is paste
		else if(m.op == 1)
		{
		    pthread_rwlock_rdlock(&clipboard_locks[i]); // critical region begins
		    m.len_data = len_messages[m.region];	// checks number of bytes occupied by the data byte array in m.region of the clipboard
		    //allocs memory for receiving the byte data array
		    data[1] = (char*) malloc(m.len_data);
		    if(data[1] == NULL)
		    {
		    	printf("Failed allocating memory.\n");
		    	free(data[0]);
		    	free(data);
		    	close(c);
		    	pthread_exit(NULL);
		    }
		    //copies the data byte array from the m.clipboard
		    memcpy(data[1], clipboard[m.region], m.len_data);
		    pthread_rwlock_unlock(&clipboard_locks[i]); //critical region end

		    totalbytes = 0;
		    //copies the contents from Message so they can be sent to client app
		    memcpy(data[0], &m, len_message);
		    //sends Message to client app
		    while(totalbytes < len_message)
		    {
		    	if((nbytes = send(c, data[0] + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
		    	{
		    		perror("Failed to send message to App");
		    		free(data[0]);
		    		free(data[1]);
		    		free(data);
		    		close(c);
		    		pthread_exit(NULL);
		    	}
		    	totalbytes += nbytes;
		    }

		    totalbytes = 0;
		    //sends data byte array to client app
		    while(totalbytes < m.len_data)
		    {
		    	if((nbytes = send(c, data[1] + totalbytes, m.len_data - totalbytes, MSG_NOSIGNAL)) == -1)
		    	{
		    		perror("Failed to send byte array to App");
		    		free(data[0]);
		    		free(data[1]);
		    		free(data);
		    		close(c);
		    		pthread_exit(NULL);
		    	}
		    	totalbytes += nbytes;
		    }
		    //frees data byte array
		    free(data[1]);

		}
		//if operation is wait
		else if(m.op == 2)
		{
			//wait for changes in m.region
			pthread_mutex_lock(&wait_muxs[m.region]);
			pthread_cond_wait(&cond_w[m.region], &wait_muxs[m.region]);
			pthread_mutex_unlock(&wait_muxs[m.region]);

			pthread_rwlock_rdlock(&clipboard_locks[i]); //critical region begins
			m.len_data = len_messages[m.region]; // checks number of bytes occupied by the data byte array in m.region of the clipboard
			//allocs memory for receiving the byte data array
			data[1] = (char*) malloc(m.len_data);
			if(data[1] == NULL)
			{
				printf("Failed allocating memory.\n");
				free(data[0]);
				free(data);
				close(c);
				pthread_exit(NULL);
			}
			//copies the data byte array from the clipboard m.region
			memcpy(data[1], clipboard[m.region], m.len_data);
		    pthread_rwlock_unlock(&clipboard_locks[i]); //critical region end

		    totalbytes = 0;
			//copies the contents from Message so they can be sent to client app
		    memcpy(data[0], &m, len_message);
		    //sends Message to client app
		    while(totalbytes < len_message)
		    {
		    	if((nbytes = send(c, data[0] + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
		    	{
		    		perror("Failed to send message to App");
		    		free(data[0]);
		    		free(data[1]);
		    		free(data);
		    		close(c);
		    		pthread_exit(NULL);
		    	}
		    	totalbytes += nbytes;
		    }

		    totalbytes = 0;
		    //sends data byte array to client app
		    while(totalbytes < m.len_data)
		    {
		    	if((nbytes = send(c, data[1] + totalbytes, m.len_data - totalbytes, MSG_NOSIGNAL)) == -1)
		    	{
		    		perror("Failed to send byte array to App");
		    		free(data[0]);
		    		free(data[1]);
		    		free(data);
		    		close(c);
		    		pthread_exit(NULL);
		    	}
		    	totalbytes += nbytes;
		    }
		    //frees data byte array
		    free(data[1]);
		}
	}
	//close file descriptor, frees memory and exits thread
	close(c);
	free(data[0]);
	free(data);
	pthread_exit(NULL);
}
