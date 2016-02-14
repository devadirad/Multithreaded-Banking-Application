#Compiler
	CC=gcc

#CFLAGS
	CFLAGS= -g -Wall -Wno-unused

bank: server client
	
server: server.o  
	$(CC) $(CFLAGS) -pthread -o  $@ $^
	
server.o: server.c 
	$(CC) -c $(CFLAGS) server.c


client: client.o  
	$(CC) $(CFLAGS) -pthread -o  $@ $^
	
client.o: client.c 
	$(CC) -c $(CFLAGS) client.c 
		
#clean
clean:
	rm -f *.o client server
