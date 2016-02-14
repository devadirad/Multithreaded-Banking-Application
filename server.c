#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>	
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/ipc.h>
# define SHM_SIZE 1024
# define BUFFSIZE 130

typedef struct account{  //account struct to create bank
	char accountName[100];
	float balance;
	int inSession;
} account;

static account list[20]={ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {} }; //initialize bank as global var
int currAccount=-1; //number of account being currently worked on
int maxAccounts=20; //how many accounts we can have in the bank
int totalAccounts=0; //the total number of accounts opened, can't exceed maxAccounts

pthread_mutex_t mutex[20]={}; //locks for bank
pthread_mutex_t * mptr;
pthread_mutexattr_t matr;

int mshared_mem_id; //shared mem stuff

int AccountSearch(account list[20], char name[100]) //looks for a certain account by name in the provided list, return index of account if found, else -1
{
	int i;
    for(i = 0;i<maxAccounts;i++){
        /* if this index contains the given value, return the index */
            if (strcmp(list[i].accountName, name)== 0){ return 
                i;}
    }
        /* if we went through the entire list and didn't find the 
         * value, then return -1 signifying that the value wasn't found
         */
        return -1;
}

int FindSpace(account list[20]) //returns free index in bank, or returns -1 if no free space
{
	int i;
    for(i= 0;i<maxAccounts;i++){
        /* if this index is free, return the index */
            if (strcmp(list[i].accountName, "")== 0) return i;
    }
    //else
    return -1;
}

int StartSession(account list[20], char name[100], pthread_mutex_t *mptrCHILD) //starts an account session by changing inSession flag to 1. returns 1 if successful, 0 otherwise.
{
	int i;
    for(i= 0;i<maxAccounts;i++){
        /* if this index contains the right account, toggle session */
            if (strcmp(list[i].accountName, name)== 0){
            	printf("Starting session for account %d!\n",i);
				pthread_mutex_lock(&mptrCHILD[i]);
                list[i].inSession=1;
                currAccount=i;
                return 1;
            }
    }
        /* if we went through the entire list and didn't find the 
         * account, then return 0
         */
        printf("Account does not exist, can't start.\n");
        return 0;
}

int FinishSession(account list[20], pthread_mutex_t *mptrCHILD) //ends an account session by changing inSession flag to 0. returns 1 if successful, 0 otherwise.
{
				printf("Finished session!\n");
                list[currAccount].inSession=0;
                pthread_mutex_unlock(&mptrCHILD[currAccount]);
				currAccount=-1;
                return 1;
}

int Credit(account list[20], float amount, int sockfd) //adds money to an account. returns 1 if successful, 0 otherwise.
{
                list[currAccount].balance = list[currAccount].balance + amount;
				write(sockfd, "Credited\n.",strlen("Credited\n."));
                return 1;
}

int Debit(account list[20], float amount, int sockfd) //subtracts money to an account. returns 1 if successful, 0 otherwise.
{
                //if trying to withdraw more than you can (sniffle, you're broke)
                if(amount > list[currAccount].balance){
                    char* message="There is not enough balance to debit this amount.\n";
                    write(sockfd, message,strlen(message));
                    return 0;
                }

                //otherwise, hooray! get some $$$$
                list[currAccount].balance = list[currAccount].balance - amount;
				write(sockfd, "Debited\n.",strlen("Debited\n."));
                return 1;           
}

int OpenAccount(account list[20], char name[100], int sockfd) //opens and inserts a new account, returns 1 if successful
{
    int tempsearch = AccountSearch(list, name);
    if(tempsearch == -1){
        int tempindex = FindSpace(list);
        if(tempindex==-1){
			char* message="Cannot insert, already 20 accounts in the bank\n";
			write(sockfd, message,strlen(message));
    		return 0;
        }
        strcpy(list[tempindex].accountName, name); //open account
        totalAccounts++; // increment account total
        char* message="Account creation successful\n";
    	write(sockfd, message, strlen(message));
        return 1;
    }
    else{
    	char* message="Account exists! Unsuccessful.\n";
    	write(sockfd, message, strlen(message));
    }
    
   return 0;
}

void FunctionPicker(char* input, account list[20], int sockfd, pthread_mutex_t *mptrCHILD) //deals with commands, parses through and goes to bank functions
{

	char * token;
 	token = strtok (input," ");

	if(strcmp(token, "open")==0){ //open account
		if(currAccount==-1){
			 	token = strtok (NULL," ");	//read the name of account
			 	char name[100];
			 	strcpy(name, token);
			 	OpenAccount(list, name, sockfd);
		}

			else
			{
				char* message="Cannot open new account until this session is finished\n";
				write(sockfd, message, strlen(message));
			}
	}
	else if(strcmp(token, "start")==0){ //start account
		if(currAccount!=-1){
        		char* message="Cannot start new account until this session is finished\n";
				write(sockfd, message, strlen(message));
        }
        else{
        	token = strtok (NULL," ");	//read the name of account
		 	char name[100];
		 	strcpy(name, token);
		 	if(StartSession(list, name, mptrCHILD)){
		 		char* message="Account started!\n";
				write(sockfd, message, strlen(message));
		 	}
		 	else{
		 		char* message="Couldn't start account...error.\n";
				write(sockfd, message, strlen(message));
		 	}

        }
	}
	else if(strcmp(token, "credit")==0){ //credit account
		if(currAccount!=-1){
			token = strtok (NULL," ");	//get amount
			float amount = atof(token);
			Credit(list, amount, sockfd);
			char* message="Credited.\n";
			write(sockfd, message, strlen(message));
		}
		else{
			char* message="Please start a session first.\n";
			write(sockfd, message, strlen(message));
		}
	}
	else if(strcmp(token, "debit")==0){ //debit account
		if(currAccount!=-1)
		{
			token = strtok (NULL," ");	//get amount
			float amount = atof(token);
			Debit(list, amount, sockfd);
		}
		else
		{
			char* message="Please start a session first.\n";
			write(sockfd, message, strlen(message));	
		}
	}
	else if(strcmp(token, "balance")==0){ //balance account
		char* message=(char*)malloc(1000*sizeof(char));
		if(currAccount!=-1){
			sprintf(message,"Account balance= %8.2f \n",list[currAccount].balance);
			write(sockfd, message, strlen(message));
		}
		else{
			sprintf(message,"Not in session. Cannot print balance\n");
			write(sockfd, message, strlen(message));
		}
    	free(message);
	}
	else if(strcmp(token, "finish")==0){ //finish account
		if(currAccount==-1){
			char* message="Please start a session first.\n";
			write(sockfd, message, strlen(message));
		}
		else if(FinishSession(list, mptrCHILD)){
			char* message="Session finished.\n";
			write(sockfd, message, strlen(message));
		}
		else{
			char* message="Error...\n";
			write(sockfd, message, strlen(message));
		}
		}
	else{
			char* message="Error...please enter a valid command.\n"; //if user enters some random stuff
			write(sockfd, message, strlen(message));
		}
}

void* printAccounts(void* n) //print the bank every 20 seconds in its own thread
{
	key_t key; //to access shared mem
    if ((key = ftok("server.c", '8')) == -1) {
        exit(1);
    }
	account* list;
	int shmid;

	while(1)
	{
		sleep(20); //print every 20 seconds
		printf("\n");
    	printf("\n");
    	printf("=========================== BANK ACCOUNTS ===========================\n");
    	printf("\n");
		/* connect to (and possibly create) the segment: */
    	if ((shmid = shmget(key, SHM_SIZE, 0777 | IPC_CREAT)) == -1) {
        	exit(1);
    	}
    	/* attach to the segment to get a pointer to it: */
    	list =(account*) shmat(shmid, (void *)0, 0);
    	if (list == (account*)(-1)) {
        	exit(1);
		}
		printf("SMS Attached in main!\n");
		int i;
		for(i=0;i<maxAccounts;++i)
		{
			printf("Name: %s  ",list[i].accountName);
			if(list[i].inSession==1)
				printf("IN SESSION");
			printf("    Balance:%8.2f\n",list[i].balance); //printing all the relevant information
		}
			printf("\n");
    		printf("=====================================================================\n");
   			printf("\n");
    	if (shmdt(list) == -1) {
        	exit(1);
    	}
	}
	return NULL;	
}

void ProcessFunction(int sockfd,account data[20], pthread_mutex_t *mptrCHILD) //every client gets its own process that runs these commands
{
	printf("Executing in child process!\n");
	char*buffer=(char*)malloc(BUFFSIZE*sizeof(char));
	while(1)
	{
		bzero(buffer, BUFFSIZE); //clean out the buffer
		read(sockfd, buffer, BUFFSIZE);
		if ((strlen(buffer)>0) && (buffer[strlen (buffer) - 1] == '\n'))
        	buffer[strlen (buffer) - 1] = '\0';
		if(strcmp("exit",buffer)==0)
		{
			if(currAccount!=-1)
				FinishSession(data, mptrCHILD);		
			break;
		}
		FunctionPicker(buffer, data, sockfd, mptrCHILD); //parse user commands
	}
	printf("Client wants to close connection\n"); //when client wants to leave :'(
	if (shmdt(data) == -1) {
        	exit(1);
    }
    if (shmdt(mptrCHILD) == -1) {
        	exit(1);
    	}

	close(sockfd);
	return;
}

void error(const char *msg) //exits and outputs errors
{
    perror(msg);
    exit(1);
}

int main() //executes bank
{
	printf("Server has started...\n"); //and it begins

	size_t mshm_size;
	key_t mkey, key;
	int k, z, i, shmid;
	account* data;
	struct addrinfo request, *servinfo; //initialize getaddrinfo stuff
	mkey = ftok("server.c", '7');
	
	mshm_size = 20*sizeof(pthread_mutex_t); //initialize shared memory stuff
	if((mshared_mem_id = shmget(mkey, mshm_size, 0777 | IPC_CREAT)) ==-1) exit(1);
	else if((mptr = (pthread_mutex_t *)shmat(mshared_mem_id, (void *)0, 0))==NULL) exit(1);

	for(k=0;k<20;++k) mptr[k]=mutex[k];

	if(pthread_mutexattr_init(&matr)!=0) exit(1);
	else if (pthread_mutexattr_setpshared(&matr,PTHREAD_PROCESS_SHARED) !=0) exit(1);
	
	for(z=0; z<20; z++)
	{
	    if (pthread_mutex_init(&mptr[z], &matr)!= 0)
			exit(1);
	}

	if (shmdt(mptr) == -1) exit(1);
   	if ((key = ftok("server.c", '8')) == -1) exit(1); //make key
   	if ((shmid = shmget(key, SHM_SIZE, 0777 | IPC_CREAT)) == -1) exit(1); //connect to (and possibly create) the segment

    data =(account*) shmat(shmid, (void *)0, 0); //attach to the segment to get a pointer to it:
    if (data == (account*)(-1)) exit(1);

	for(i=0;i<20;++i) data[i]=list[i];

    if (shmdt(data) == -1) exit(1); //detach from the segment

	pthread_t tid;  //thread for printing out account details every 20 seconds
	pthread_create(&tid,NULL,&printAccounts,0);

	request.ai_flags = AI_PASSIVE;
	request.ai_family = AF_INET;
	request.ai_socktype = SOCK_STREAM;
	int on=1;

	request.ai_protocol=0;
	request.ai_addrlen=0;
	int sd;
	
	request.ai_addr=NULL;
	request.ai_canonname=NULL;
	request.ai_next=NULL;

	getaddrinfo(NULL, "11115", &request, &servinfo); //getaddrinfo stuff to connect to socket
	sd=socket(servinfo->ai_family,servinfo->ai_socktype, servinfo->ai_protocol);
	setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(int));
	bind(sd, servinfo->ai_addr, servinfo->ai_addrlen);
	listen(sd, 100); //wait for connections from clients
		int fd, pid;
		struct sockaddr_in senderAddr;
		unsigned int ic=sizeof(senderAddr);
		while((fd=accept(sd,(struct sockaddr *)&senderAddr,&ic))) //while loop that spawns new processes
		{
			pid = fork();
			if(pid == 0)
			{
				close(sd);
    				data =(account*) shmat(shmid, (void *)0, 0); //attach to the segment to get a pointer to it
    				if (data == (account*)(-1)) exit(1);
    				printf("Client connnected!\n"); //can start transaction operations

				pthread_mutex_t *mdata =(pthread_mutex_t *) shmat(mshared_mem_id, (void *)0, 0);
    				if (mdata == (pthread_mutex_t*)(-1)) 
				{
        				exit(1);
    				}

   					ProcessFunction(fd, data, mdata); //do stuff for each process as defined
					exit(0);
			}
			else close(fd);
		}	
}
