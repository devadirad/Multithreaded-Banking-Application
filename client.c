#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <signal.h>

# define BUFFSIZE 130

static int commandControl; //switch that controls thread synchronization
char startedsession[100];  //hold the name of the session started. If in case signint occurs use this to finish the session
int finished =-1;	       //flag -1 -> session not started 0 -> session started not finished 1->session finished
int sockd;				   //temp global variable to finish the session on sigint

void error(const char *msg) //function used to report errors with an appropriate message
{
    perror(msg);
    exit(0);
}

void sig_handler(int signo) 
{
char finbuffer[BUFFSIZE]="finish ";
if(signo==SIGINT) {
	if(finished==0)
	{
	printf("\nExititing client abruptly.. finishing started session\n");
	write(sockd,strcat(finbuffer,startedsession),BUFFSIZE);
	exit(0);
	}
	else
	{
	printf("\nExiting client\n");
	exit(0);
	}
 }
}

int checkConnection(int sd) //spammed periodically to check whether the server is still running, otherwise terminates client.
{
	int error = 0;
	socklen_t len = sizeof (error);
	int retval = getsockopt (sd, SOL_SOCKET, SO_ERROR, &error, &len);

	if (retval != 0) {
		/* there was a problem getting the error code */
		exit(0);
		return 0;
	}

	else if (error != 0) {
		/* socket has a non zero error status */
		exit(0);
		return 0;
	}
	else return 1;
}

void * readClient(void *fd) //thread function to read client input
{
	int sd = *(int*)fd;
	char clibuff[BUFFSIZE]="";
	char *token="";
	while(strcmp(token,"exit\n\0")!=0) //while exit condition is not provided by the user
	{
		bzero(clibuff, BUFFSIZE); //clearing out the buffer to recieve the command
		int size_input=read(0, clibuff, BUFFSIZE);
		if(!checkConnection(sd)) exit(0);
		
		//update information if command is start
		if(strncmp(clibuff,"start",5)==0){
			strncpy(startedsession,clibuff+6,size_input-6);		// store the name of session
			startedsession[size_input-4]=='\0';
			finished=0;											//session started, not finished
			sockd=sd;											//update global variable 
			}
		else if(strncmp(clibuff,"finish",6)==0)					//update finished flag to 1
			finished=1;
			
		write(sd, clibuff, size_input);
		sleep(2);
		if(!checkConnection(sd)) exit(0);
		token=strtok(clibuff, "0");
	}	
	commandControl=1; //thread sync
	exit(0);
	return 0;
}

void * readServer(void *fd) //thread function to read the messages from the server for the user
{
	int sd = *(int*)fd;
	char servbuff[BUFFSIZE]="";
	while(commandControl!=1) //thread sync
	{
		if(!checkConnection(sd)) exit(0);
		bzero(servbuff, BUFFSIZE);
		while(read(sd, servbuff,BUFFSIZE)==0 && commandControl!=1)
		{
			if(!checkConnection(sd)) exit(0); //chilling while waiting instruction to be put in
			sleep(5);
		}
		if(commandControl==1)
			break;
		printf("Message from server:  %s\n",servbuff); //output message from server
		if(!checkConnection(sd)) exit(0);
	}
	close(sd);
	return 0;
}

int main(int argc, char** argv)
{
    signal(SIGINT,sig_handler);
	int connected = 1;
	int sd;
	while(connected){ //loop to retry connecting to server if failed
		int status;
		struct addrinfo request, *result;

		memset(&request, 0, sizeof request); //get addr info stuff to get the connection
		request.ai_family = AF_INET; 
		request.ai_socktype = SOCK_STREAM;
		status = getaddrinfo(argv[1], "11115", &request, &result);
		sd=socket(result->ai_family,result->ai_socktype, result->ai_protocol);
		int on=1;
		setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(int));
		int stat;
		on--;
		if((stat = connect(sd, result->ai_addr, result->ai_addrlen)) == -1 ) //checks if connected
		{
			printf("Can't connect, retrying....\n");
			sleep(3);
		}
		else{
			connected =0;
		}


	}

	printf("Connected! Enter your commands now:\n"); //hooray
	pthread_t thread1; //initialize threads
	pthread_t thread2;
    pthread_create(&thread1, NULL, &readClient, (void *)&sd); //for reading messages
	pthread_create(&thread2, NULL, &readServer, (void *)&sd); //for getting replies
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
    return 0;
}


