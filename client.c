#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 200
#define MODULO 128
#define NAME_SIZE 20

//error

void error(const char *msg)
{
    perror(msg);
    printf("closing connection with the server\n");
    exit(0);
}

// remove from string

void string_trimln(char* message) 
{
    char* first_newline = strchr(message, '\n');
    if (first_newline)
        *first_newline = '\0';
}

// generate public key

uint8_t generate_public_key()
{
    srand(time(0));
    uint8_t public_key = rand()%MODULO;
    return public_key;
}

// generate private key

uint8_t generate_private_key(uint8_t public_key)
{
    uint8_t key = MODULO - public_key;
    return key;
}

// Encrypt function for messages

char* encrypt(uint8_t public_key,char* message)
{
    //printf("original message=%s\n",message);
    int n = strlen(message);
    char* temp = (char *)malloc(n+1);
    strcpy(temp, message);

    for (int i=0;i<n;i++){
        char c = (temp[i] + public_key) % MODULO;
        temp[i]=c;
    }
    //printf("Encrypted message=%s\n",temp);
    return (char *)temp;
}

// Decrypt funtion for messsages

void decrypt(uint8_t private_key,char* message)
{
    //printf("message to de deccrypted=%s\n",message);
    int n = strlen(message);

    for (int i=0;i<n;i++){
        char c = (message[i] + private_key) % MODULO;
        message[i]=c;
    }
    //printf("message after decryption=%s\n",message);
}

// global variables

char name[NAME_SIZE];
int sockfd = 0;
pthread_t tmp_thread;
uint8_t my_public_key;
uint8_t my_private_key;
uint8_t user_public_key;
char user_name[NAME_SIZE];

// function for sending message
void str_overwrite_stdout() 
{
    printf("%s", ">");
    fflush(stdout);
}

void *send_message() 
{

    char message[BUFFER_SIZE];
	char buffer[BUFFER_SIZE + NAME_SIZE + 4];
    char *encrypted_message;

    while(1) 
    {

        bzero(message,BUFFER_SIZE);
        bzero(buffer,BUFFER_SIZE + NAME_SIZE + 4 );
        str_overwrite_stdout();
        fgets(message,BUFFER_SIZE, stdin);
        string_trimln(message);

        int i = strncmp("exit",message,4);
        if(i==0)
        {
            write(sockfd,message,strlen(message));
            fflush(stdout);
            pthread_cancel(tmp_thread);
            pthread_exit(NULL);
            break;
        }
            

        sprintf(buffer, "%s : %s", name, message);
        //printf("\nEntered message = %s",buffer);

        //encrypt message
        encrypted_message = encrypt(user_public_key,buffer);
        int n = write(sockfd,encrypted_message,strlen(encrypted_message));
        if (n < 0)
    	    error("ERROR writing to socket");

    }
}

// function for receiving message

void *recieve_message() 
{
	char message[BUFFER_SIZE];
    tmp_thread = pthread_self();

    while (1) 
    {
        bzero(message, BUFFER_SIZE);
        fflush(stdout);
        // read message

        int n = read( sockfd, message, BUFFER_SIZE );
        if (n < 0)
            error("ERROR reading from socket"); 

        // decrypt

        decrypt(my_private_key,message); 

        //printf("Message received");
        printf("%s\n", message); 
        str_overwrite_stdout();
    }
}

//get public, private keys and usernaame

void presteps_chat(int sockfd)
{
    int n;
    // enter your user name

    printf("Enter your username : ");
    fgets(name,NAME_SIZE,stdin);
    string_trimln(name);
    
    //send name
    n = write(sockfd,name,strlen(name));
    if (n < 0)
    	error("ERROR writing to socket");

    my_public_key = generate_public_key();

    my_private_key = generate_private_key(my_public_key);

    // send public key
    n = write(sockfd,&my_public_key,sizeof(uint8_t));
    if (n < 0)
        error("ERROR reading from socket");

}

// get active users and return choice

int get_users(int sockfd)
{
    int n;
    
    // read and print all users available
    printf("\n*********Users available*********\n");

    int i=0;

    while (1)
    {
        char buffer[100];
        int temp;

        n = read(sockfd,&temp,sizeof(int));
        if (n < 0)
            error("ERROR reading from socket");

        if(temp==-1)
            break;

        n=read(sockfd,buffer,100);
        if (n < 0)
            error("ERROR reading from socket");
        string_trimln(buffer);

        printf("[%d] Chat with %s\n",temp,buffer);
        i++;
    }

    
    printf("\nUsers available for chat: %d\n",i);

    printf("\n[0] Exit program\n");
    printf("[1] Refresh\n");

    int choice;

    do
    {
        printf("\nEnter your choice(Ex. 1,2,0) : ");
        scanf("%d",&choice);
    } while (choice<0);
    
    // send choice to server

    n = write(sockfd,&choice,sizeof(int));
    if (n < 0)
    	error("ERROR writing to socket");

    return choice;
}

// main funtion

int main(int argc, char *argv[])
{
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[100];
    
    if (argc < 3) 
    {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);

    // create stream socket

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);

    if (server == NULL) 
    {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    //build server address structure

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    //connect to server

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    else
        printf("\n***********Chat App started successfully***********\n");

    presteps_chat(sockfd);

    int choice;

    while(1)
    {
        
        choice=get_users(sockfd);

        if(choice==0)
            break;
        
        if(choice!=1)
        {
            bool start=false;
            int response;

            //read status from server
            printf("Waiting for %s response.please wait",user_name);
            n = read(sockfd,&response,sizeof(int));
            if (n < 0)
                error("ERROR reading from socket");

            if(response==1)
                start=true;

            if(start==true)
            {
                // get user name of other user
                n = read(sockfd,user_name,NAME_SIZE);
                if (n < 0)
                    error("ERROR reading from socket");
                string_trimln(user_name);

                // get public key of other user
                n = read(sockfd,&user_public_key,sizeof(uint8_t));
                if (n < 0)
                    error("ERROR reading from socket");

                printf("***********Chat with %s started***********",user_name);

                pthread_t send_message_thread;
                pthread_t recieve_message_thread;

                pthread_create(&send_message_thread, NULL, &send_message, NULL);
                pthread_create(&recieve_message_thread, NULL, &recieve_message, NULL);

                pthread_join(send_message_thread,NULL);
                pthread_join(recieve_message_thread,NULL);
            }
            else
            {
                if(response==2)
                    printf("Timeout.No response from user!!! Connection with %s failed. please try again",user_name);
                else if(response==3)
                    printf("User busy or not available!!! Connection with %s failed. please try again",user_name);
            }
        }
           
    }
    
	close(sockfd);

	return 0;
}
