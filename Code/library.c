#include "clip_implementation.h"

//function used to connect client app to local clipboard using AF_UNIX
int clipboard_connect(char * clipboard_dir)
{
    //Socket variables
    int s = 0;
    struct sockaddr_un addr;
    socklen_t addr_size = sizeof(struct sockaddr_un);
	//File descriptor creation
    if((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
    	perror("Failed creating socket");
    	return s;
    }
	//sets all addr memory to 0
    memset(&addr, 0, sizeof(struct sockaddr_un));
	//socket type and file location
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, clipboard_dir, sizeof(addr.sun_path) -1);
	//connection to clipboard
    if(connect(s, (struct sockaddr *) &addr, addr_size) == -1)
    {
    	perror("Failed connecting socket");
    	return -1;
    } 
	//returns file descriptor that handles connection between app and local clipboard
    return s;
}

//function used to copy data pointed to by buf from client app to region in a region of the local clipboard
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count)
{
	//Variables used to exchange data between processes
    Message m;
    int nbytes = 0, totalbytes = 0, len_message = sizeof(Message);
    //checks if input region is valid
    if(region < 0 || region > N_CLIP_REG - 1)
    {
        printf("Invalid region!\n");
        return 0;
    }
	//allocates memory to send Message as a byte array
    void* msg = malloc(len_message);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
	//set Message member variables accordingly
    m.region = region;
    m.op = 0;  // 0 is copy;
    m.len_data = count;	
	//copy the contents of Message to message byte array
    memcpy(msg, (void*)&m, len_message);

    // sends operation ID and information about the data byte array that will be sent
    while(totalbytes < len_message)
    {
    	if((nbytes = send(clipboard_id, msg + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
    	{
    	    perror("Failed sending message to clip");
            free(msg);
    	    return 0;
    	}
        totalbytes += nbytes;
    }
	//free byte array so it can be reallocated
    free(msg);
	//allocate memory for data byte array
    msg  = malloc(count);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
	//copy the contents of buf to data byte array, up to count bytes
    memcpy(msg, buf, count);
    totalbytes = 0;	

    // sends the data byte array
    while(totalbytes < count)
    {
    	if((nbytes = send(clipboard_id, msg + totalbytes, count - totalbytes, MSG_NOSIGNAL)) == -1)
    	{
    	    perror("Failed to send byte array to clip");
    	    free(msg);
            return 0;
    	}
	totalbytes += nbytes;
    }
	//free used variables and return number of bytes sent
    free(msg);
    return count;
}

//function used to paste data stored in a region of the local clipboard to the client app through buf
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count)
{
    //Variables used to exchange data between processes
    Message m;
    int nbytes = 0, totalbytes = 0, len_message = sizeof(Message);
    //checks if input region is valid
    if(region < 0 || region > N_CLIP_REG - 1)
    {
        printf("Invalid region!\n");
        return 0;
    }
    //allocates memory to send Message as a byte array
    void* msg = (void*)malloc(len_message);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
    //set Message member variables accordingly
    m.region = region;
    m.op = 1;  //1 is paste;
    m.len_data = 0;
    //copy the contents of Message to message byte array
    memcpy(msg, &m, len_message);

    // sends operation ID and the region of the data byte array required
    while(totalbytes < len_message)
    {
    	if((nbytes = send(clipboard_id, msg + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
        {
    	    perror("Failed sending message to clip");
            free(msg);
    	    return 0;
        }
    	totalbytes += nbytes;
    }

    totalbytes = 0;
	
    // receives information about the data byte array that will be received, namely the lenght of the data byte array
    while(totalbytes < len_message)
    {
        if((nbytes = recv(clipboard_id, msg + totalbytes, len_message - totalbytes, 0)) == -1)
    	{
    	    perror("Error receiving message from clip");
    	    free(msg);
    	    return 0;
    	}
        else if(nbytes == 0) break;
        totalbytes += nbytes;
    }
    // verifies if the connection with the clipboard was lost
    if(nbytes == 0)
    {
    	printf("Connection with clipboard lost...\n");
    	clipboard_close(clipboard_id);
    	free(msg);
    	return 0;
    }

    //updates Message, containing now the data byte array information
    memcpy(&m, msg, len_message);
    //free byte array so it can be reallocated
    free(msg);
    //allocate memory for data byte array
    msg = malloc(m.len_data);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
    
    totalbytes = 0;
    
    // receives the data byte array
    while(totalbytes < m.len_data)
    {
        if((nbytes = recv(clipboard_id, msg + totalbytes, m.len_data - totalbytes, 0)) == -1)
        {
            perror("Error receiving byte array from clip");
            free(msg);
            return 0;
        }
        else if(nbytes == 0) break;
        totalbytes += nbytes;
    }
    // verifies if the connection with the clipboard was lost
    if(nbytes == 0)
    {
    	printf("Connection with clipboard lost...\n");
    	clipboard_close(clipboard_id);
    	free(msg);
    	return 0;
    }
    // picks how many bytes will be sent to the client app
    // If the data byte array received has more bytes than count, the number of bytes sent is equal to count.
    // This guarantees that the client app only receives at maximum the number of bytes it asked for.
    if(m.len_data > count)
        memcpy(buf, msg, count);
    // If the data byte array received has less bytes than count, the number of bytes sent is equal to the number of bytes received at data byte array.
    // This way the client app doesn't receive more bytes that the ones it needs to receive.
    else
        memcpy(buf, msg, m.len_data);
    //free used variables and return the correct number of bytes received
    free(msg);
    return (m.len_data > count)? count : m.len_data;
}

//function used to paste data stored in a region of the local clipboard to the client app through buf, after the same region has been updated
int clipboard_wait(int clipboard_id, int region, void* buf, size_t count)
{
    //Variables used to exchange data between processes
    Message m;
    int nbytes = 0, totalbytes = 0, len_message = sizeof(Message);
    //checks if input region is valid
    if(region < 0 || region > N_CLIP_REG -1)
    {
        printf("Invalid region!\n");
        return 0;
    }
    //allocates memory to send Message as a byte array
    void* msg = (void*)malloc(len_message);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
    //set Message member variables accordingly
    m.region = region;
    m.op = 2;  // 2 is wait;
    m.len_data = 0;
    //copy the contents of Message to message byte array
    memcpy(msg, &m, len_message);
    // sends operation ID and the region of the data byte array required
    while(totalbytes < len_message)
    {
        if((nbytes = send(clipboard_id, msg + totalbytes, len_message - totalbytes, MSG_NOSIGNAL)) == -1)
        {
            perror("Failed sending message to clip");
            free(msg);
            return 0;
        }
        totalbytes += nbytes;
    }

    printf("Waiting for region to change.\n");

    totalbytes = 0;
    // the function will be stuck in recv until data in m.region has been changed by any other client app, local or not
    // when data updates, receives information about the data byte array that will be received, namely the lenght of the data byte array
    while(totalbytes < len_message)
    {
        if((nbytes = recv(clipboard_id, msg + totalbytes, len_message - totalbytes, 0)) == -1)
        {
            perror("Error receiving message from clip");
            free(msg);
            return 0;
        }
        else if(nbytes == 0) break;
        totalbytes += nbytes;
    }
    // verifies if the connection with the clipboard was lost
    if(nbytes == 0)
    {
        printf("Connection with clipboard lost...\n");
        clipboard_close(clipboard_id);
        free(msg);
        return 0;
    }
    //updates Message, containing now the data byte array information
    memcpy(&m, msg, len_message);
    //free byte array so it can be reallocated
    free(msg);
    //allocate memory for data byte array
    msg = malloc(m.len_data);
    if(msg == NULL)
    {
        printf("Failed allocating memory.\n");
        return 0;
    }
    
    totalbytes = 0;
    
    // receives the data byte array
    while(totalbytes < m.len_data)
    {
        if((nbytes = recv(clipboard_id, msg + totalbytes, m.len_data - totalbytes, 0)) == -1)
        {
            perror("Error receiving byte array from clip");
            free(msg);
            return 0;
        }
        else if(nbytes == 0) break;
        totalbytes += nbytes;
    }
    // verifies if the connection with the clipboard was lost
    if(nbytes == 0)
    {
        printf("Connection with clipboard lost...\n");
        clipboard_close(clipboard_id);
        free(msg);
        return 0;
    }
    // picks how many bytes will be sent to the client app
    // If the data byte array received has more bytes than count, the number of bytes sent is equal to count.
    // This guarantees that the client app only receives at maximum the number of bytes it asked for.
    if(m.len_data > count)
        memcpy(buf, msg, count);
    else
        memcpy(buf, msg, m.len_data);
    //free used variables and return the correct number of bytes received
    free(msg);
    return (m.len_data > count)? count : m.len_data;
}

//closes the clipboard file descriptor 
void clipboard_close(int clipboard_id)
{
    if(close(clipboard_id) == -1) perror("Error closing clipboard file descriptor");
}
