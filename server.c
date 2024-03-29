#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define MAX_CLIENTS 20
#define BUFFER_SIZE 200
#define MODULO 128
#define NAME_SIZE 20

// error

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

// Client structure 

typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int uid_self;
    int uid_user;
    uint8_t public_key;
	char name[NAME_SIZE];
} client_t;

// global variables

client_t *clients[MAX_CLIENTS];
static _Atomic unsigned int cli_count = 0;
static int uid = 10;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// print ip address

void print_client_addr(struct sockaddr_in addr)
{
    printf("%d.%d.%d.%d",
         addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// remove \n from string

void string_trimln(char* message) 
{
    char* first_newline = strchr(message, '\n');
    if (first_newline)
        *first_newline = '\0';
}

// add client to queue

void queue_add(client_t *client_structure)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(!clients[i])
        {
			clients[i] = client_structure;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Remove clients from queue

void queue_remove(int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid_self == uid)
            {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Send message to client

void send_message(char *message, int uid)
{
    int n;
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid_self == uid)
            {
				n = write(clients[i]->sockfd,message,strlen(message));
        	    if (n < 0)
            	    printf("ERROR writing to socket to user id=%d",uid);
                break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//send user list to user

void send_list(int sockfd, int uid)
{
    
	pthread_mutex_lock(&clients_mutex);

    int n;

	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid_self != uid)
            {
                n = write(sockfd,&(clients[i]->uid_self),sizeof(int));
                if (n < 0)
                    error("ERROR reading from socket");
				
                if( (clients[i]->uid_user)==uid)
                {
                    char buffer[BUFFER_SIZE];
                    sprintf(buffer, "%s : interested for chat", clients[i]->name);
                    n = write(sockfd,buffer,strlen(buffer));
        	        if (n < 0)
            	        printf("ERROR writing to socket to user id=%d",uid);
                }
                else
                {
                    n = write(sockfd,(clients[i]->name),strlen(clients[i]->name));
        	        if (n < 0)
            	        printf("ERROR writing to socket to user id=%d",uid);
                }
                
			}
		}
	}

    int temp = -1;
    n = write(sockfd,&temp,sizeof(int));
    if (n < 0)
        printf("ERROR writing to socket to user id=%d",uid);

	pthread_mutex_unlock(&clients_mutex);

    
    
}

//check if user is free
void check_connection(int* choice,int uid_target,int uid_self)
{
    
	pthread_mutex_lock(&clients_mutex);

    int result=-1;
    

	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i] && clients[i]->uid_self == uid_target)
        {
			if(clients[i]->uid_user==uid_self)
                result=1;
            else if(clients[i]->uid_user!=0)
                result=3;
            else if(clients[i]->uid_user==0)
                result=2;
            break;
		}
	}
    
    if(result==-1)
        result=3;
    *choice=result;
    pthread_mutex_unlock(&clients_mutex);
    
}

//return name of user from uid
void get_name(char *name,int uid_target)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i] && clients[i]->uid_self == uid_target)
        {
			strcpy(name ,clients[i]->name); 
            break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//return public key of user from uid
void get_publickey( uint8_t *temp,int uid_target)
{

	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i] && clients[i]->uid_self == uid_target)
        {
			*temp=clients[i]->public_key;
            break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
    
}


// Handle all communication with the client 

void *handle_client(void *arg)
{
	char buffer[BUFFER_SIZE];
	char name[NAME_SIZE];
    int n;
    char *encrypted_message;

	cli_count++;
	client_t *client_structure = (client_t *)arg;

    // send public_key to client

    //n = write( client_structure->sockfd ,&public_key,sizeof(uint8_t));
    //if (n < 0)
    //    error("ERROR writing to socket");

	// Read name of client

    n = read( client_structure->sockfd, name , NAME_SIZE);
    if (n < 0)
        error("ERROR reading from socket");
    string_trimln(name);

	strcpy(client_structure->name, name);

    // read public key of user
    n = read( client_structure->sockfd, &client_structure->public_key , sizeof(uint8_t));
    if (n < 0)
        error("ERROR reading from socket");

	// sprintf(buffer, "%s has joined", client_structure->name);

	//printf("\n%s", buffer );

    // encrypted_message = encrypt(public_key,buffer);
	// send_message(encrypted_message, client_structure->uid);

    while(1)
    {

        send_list(client_structure->sockfd, client_structure->uid_self);
        int choice;

        // read choice
        n = read(client_structure->sockfd,&choice,sizeof(int));
        if (n < 0)
            error("ERROR reading from socket");

        if(choice==0)
            break;
        if(choice!=1)
        {
            
            client_structure->uid_user = choice;

            // check and make connection
        
            int response;

            for (int i = 0; i < 15; i++)
            {
                check_connection(&response, client_structure->uid_user,client_structure->uid_user);
                if(response==1)
                    break;
                sleep(1);
            }
        
            if(response==3)
            {
                int temp=3;
                n = write( client_structure->sockfd ,&temp,sizeof(int));
                if (n < 0)
                    error("ERROR writing to socket");
                // initialize uid_user to 0
                client_structure->uid_user = 0;
            }
            else if(response==2)
            {
                int temp=2;
                n = write( client_structure->sockfd ,&temp,sizeof(int));
                if (n < 0)
                    error("ERROR writing to socket");
                // initialize uid_user to 0
                client_structure->uid_user = 0;
            }
            else
            {
            
                // send name of other user to client
                get_name(name,client_structure->uid_user);

                n = write( client_structure->sockfd ,name,strlen(name));
                if (n < 0)
                    error("ERROR writing to socket");

                //send public key of other user to client
                uint8_t temp;
                
                get_publickey(&temp,client_structure->uid_user);

                n = write( client_structure->sockfd ,&temp,sizeof(uint8_t));
                if (n < 0)
                    error("ERROR writing to socket");


                while(1)
                {
                    bzero(buffer,BUFFER_SIZE);  

		            n = read(client_structure->sockfd , buffer, BUFFER_SIZE );
                    if (n < 0)
    		            error("ERROR writing to socket");

                    int e = strncmp( "exit",buffer,4);

                    if( e!=0 && strlen(buffer) > 0 )
                    {
                        send_message(buffer, client_structure->uid_user);
                    }
		            else if ( e == 0 ) 
                    {
			            sprintf(buffer, "%s has left", client_structure->name);
			            send_message( buffer, client_structure->uid_user);
                        break;
		            } 
	            }
            }
        }
    }
	

    // Delete client from queue and yield thread

    close(client_structure->sockfd);
    queue_remove(client_structure->uid_self);
    free(client_structure);
    cli_count--;
    pthread_detach(pthread_self());

	return NULL;
}

// main funtion

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno,temp;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     pthread_t tid;

     if (argc < 2) 
     {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
        error("ERROR opening socket");

     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);

     //build server address structure

     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
     serv_addr.sin_port = htons(portno);

     //bind local port number

     if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR on binding");

     //specify number of concurrent
     //clients to listen for

     listen(sockfd,10);

     printf("\n*********Chat server started*********\n");

     while(1)
     {
    
     	//Wait for client

     	clilen = sizeof(cli_addr);
     	newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
     	if (newsockfd < 0)
        	error("ERROR on accept");
     	else
        	printf("\nReceived connection from host [IP %s TCP port %d] \n",inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));

        if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			close(newsockfd);
			continue;
		}

        client_t *client_structure = (client_t *)malloc(sizeof(client_t));
		client_structure->address = cli_addr;
		client_structure->sockfd = newsockfd;
		client_structure->uid_self = uid++;
        client_structure->uid_user = 0;

		// Add client to the queue and fork thread 

		queue_add(client_structure);
		pthread_create(&tid, NULL, &handle_client, (void*)client_structure);


		/* Reduce CPU usage */
		sleep(1);
	}

    return 0;
}