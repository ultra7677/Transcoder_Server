extern "C" {
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<json-c/json.h>
}
#include "transcoder_core.h"
#define BUFFSIZE 1024
 
void * get_in_addr(struct sockaddr * sa)
{
	if(sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main( int argc, char **argv)  
{
// Variables for writing a server.
    int status;
    struct addrinfo hints, * res;
    int listner;
// Before using hint you have to make sure that the data structure is empty
    memset(& hints, 0, sizeof hints);
// Set the attribute for hint
    hints.ai_family = AF_UNSPEC; // We don't care V4 AF_INET or 6 AF_INET6
    hints.ai_socktype = SOCK_STREAM; // TCP Socket SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;
// Fill the res data structure and make sure that the results make sense.
    status = getaddrinfo(NULL, "8888" , &hints, &res);
    if(status != 0)
    {
	fprintf(stderr,"getaddrinfo error: %s\n",gai_strerror(status));
    }
// Create Socket and check if error occured afterwards
    listner = socket(res->ai_family,res->ai_socktype, res->ai_protocol);
    if(listner < 0 )
    {
	fprintf(stderr,"socket error: %s\n",gai_strerror(status));
    }
// Bind the socket to the address of my local machine and port number
    status = bind(listner, res->ai_addr, res->ai_addrlen);
    if(status < 0)
    {
	fprintf(stderr,"bind: %s\n",gai_strerror(status));
    }

    status = listen(listner, 10);
    if(status < 0)
    {
	fprintf(stderr,"listen: %s\n",gai_strerror(status));
    }
// Free the res linked list after we are done with it
    freeaddrinfo(res);
// We should wait now for a connection to accept
	int new_conn_fd;
	struct sockaddr_storage client_addr;
	socklen_t addr_size;
	char s[INET6_ADDRSTRLEN]; // an empty string
        char buf[BUFFSIZE];
	// Calculate the size of the data structure
	addr_size = sizeof client_addr;

	printf("I am now accepting connections ...\n");

	while(1){
		// Accept a new connection and return back the socket desciptor
		new_conn_fd = accept(listner, (struct sockaddr *) & client_addr, &addr_size);
		if(new_conn_fd < 0)
		{
			fprintf(stderr,"accept: %s\n",gai_strerror(new_conn_fd));
			continue;
		}

		inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *) &client_addr),s ,sizeof s);
		printf("I am now connected to %s \n",s);

        int received = -1;
        /* Receive message */
        if ((received = recv(new_conn_fd, buf, BUFFSIZE, 0)) < 0) {
            printf("Failed to receive initial bytes from client");
        }else{
            printf("Received String from Client : %s\n",buf);
            json_object * jobj = json_tokener_parse(buf);

            // Get the command value
            json_object * command = json_object_object_get(jobj,"command");

            // Command is postVideo
            if (strcmp(json_object_get_string(command),"postVideo") == 0)
            {
                cout << json_object_get_string(json_object_object_get(jobj,"command")) << endl;
            }
            // Command is postVideoTask
     	    if (strcmp(json_object_get_string(command),"postVideoTask") == 0)
	        {
		        printf("Deal with postVideoTask \n");
		        init_task(jobj);		
	        }
           
            // Command is getVideoList
            if (strcmp(json_object_get_string(command),"getVideoList") == 0){
                printf("getVideoList \n");
		//Creating a json object
                json_object * jobj = json_object_new_object();
                //Creating a json array
                json_object * jarray = json_object_new_array();
                //Creating videoInfo objects in this array
                json_object * videoInfoObj1 = json_object_new_object();
                //Creating a json string in videoInfoObj1
                json_object * name_str = json_object_new_string("big_v.mp4");
                json_object_object_add(videoInfoObj1,"videoname",name_str);
                
                json_object * id_int = json_object_new_int(0);
                json_object_object_add(videoInfoObj1,"id",id_int);
                
                json_object * taskList_array = json_object_new_array();
                json_object_object_add(videoInfoObj1,"taskList",taskList_array);
                
                json_object_array_add(jarray,videoInfoObj1);
                json_object_object_add(jobj,"videoList",jarray);
               
                status = send(new_conn_fd,json_object_get_string(jobj),strlen(json_object_get_string(jobj)),0);
            }
        }

	//status = send(new_conn_fd,"Welcome", 7,0);
	if(status == -1)
	{
	    close(new_conn_fd);
	    _exit(4);
	}

	}
	// Close the socket before we finish
	close(new_conn_fd);
	return 0;
}
