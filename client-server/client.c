
/*
###########################################################################################################
+++*****************************************************************************************************+++
___________Client Application (client.c) - Simple Documentation____________________________________________    
    
->Introduction:
    The client application communicates with server (server.c) via the terminal. Users can give commands in
    the terminal as input(mandatory ends with '\n' ), which are sent to the server for processing. The server 
    returns the results of these commands to the client. A help function is available to list supported commands.

->Prerequisites:
    - A working C development environment.
    - The server application (server.c) should be running and reachable.

->Usage:
    - Compile: gcc client.c -o client
    - Run: ./client
->Communication with server:
    - The client sends commands to the server using FIFO files.
    - The server processes commands and sends back results using FIFO files.

->Available commands:
    - login : username
    - logout
    -get-logged-users
    -get-proc-info : process_id

->Return code:
    0  = application finished with succes
    1  = Error at creating fifo file
    2  = Error at opening fifo file
    3  = Error at closing fifo file
    4  = Error at printf
    5  = Error at fflush
    6  = Error at getting user input
    7  = Error at sending message to server
    8  = Error at receiving response prefix from server
    9  = Error at receiving message from server
    10 = Error at printing server response
*/
#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define CLIENT_SERVER_FIFO "user_commands"
#define SERVER_CLIENT_FIFO "server_processed_answers"
#define COMMAND_BUFFER_SIZE 1024
#define RESPONSE_BUFFER_SIZE 8192

#define CHECK(condition, error_id, message)\
    if ((condition))             \
    {                            \
        perror(message);         \
        exit(error_id);          \
    }                            \

//fifo descriptors
int fifo_client_server_descriptor;
int fifo_server_client_descriptor;

char input[COMMAND_BUFFER_SIZE];
char output[RESPONSE_BUFFER_SIZE];
int responseSize;

bool FIFOexists(const char* pathToFifo)
{
    struct stat st;
    return (stat(pathToFifo, &st) == 0 && S_ISFIFO(st.st_mode));
}

void openFIFOs()
{
    //client server fifo
    if(!FIFOexists(CLIENT_SERVER_FIFO)) //create fifo if it doesn't exists 
        CHECK(-1 == mkfifo(CLIENT_SERVER_FIFO, 0600), 1, "[CLIENT]:Error at creating fifo file!\n")
    CHECK(-1 == (fifo_client_server_descriptor = open(CLIENT_SERVER_FIFO, O_WRONLY)), 2, "[CLIENT]:Error at opening fifo file!\n")

    //server client fifo
    if(!FIFOexists(SERVER_CLIENT_FIFO)) //create fifo if it doesn't exists 
        CHECK(-1 == mkfifo(SERVER_CLIENT_FIFO, 0600), 1, "[CLIENT]:Error at creating fifo file!\n")
    CHECK(-1 == (fifo_server_client_descriptor = open(SERVER_CLIENT_FIFO, O_RDONLY)), 2, "[CLIENT]:Error at opening fifo file!\n")
}

void closeFIFOs()
{
    CHECK(-1 == close(fifo_client_server_descriptor), 3, "[CLIENT]:Error at closing fifo file")
    CHECK(-1 == close(fifo_server_client_descriptor), 3, "[CLIENT]:Error at closing fifo file")
}

int main()
{
    //opening fifo files for communication between client and server
    openFIFOs();

    CHECK(printf("-----Welcome to the Client Application\n") < 0, 4, "[Client]:Error at printf!")
    CHECK(printf("-------------Please input commands to interact with the server(Each command mandatory ends with '\\n').\n") < 0, 4, "[Client]:Error at printf!")
    CHECK(fflush(stdout) != 0, 5, "[CLIENT]:Error at fflush")

    //running client app until user decides to quit or an error occures
    while(true)
    {
        printf("-->");
        CHECK(fflush(stdout) != 0, 5, "[CLIENT]:Error at fflush")

        //get user input
        CHECK(NULL == fgets(input, COMMAND_BUFFER_SIZE, stdin), 6, "[CLIENT]:Error at getting user input!\n")

        //send command to server
        CHECK(-1 == write(fifo_client_server_descriptor, input, strlen(input)), 7, "[CLIENT]:Error at sending message to server!")
        
        //user wants to leave 
        if(!strcmp(input, "quit\n"))
        {
            exit(0);
        }
        
        // //receive response from server
        CHECK(-1 == read(fifo_server_client_descriptor, &responseSize, sizeof(int)), 8, "[CLIENT]:Error at receiving response prefix from server!")
        CHECK(-1 == read(fifo_server_client_descriptor, &output, responseSize), 9, "[CLIENT]:Error at receiving message from server")
        
        // printing server response
        // printf("%d %s\n", responseSize, output);
        output[responseSize] = '\0';
        CHECK(printf("-->%s", output) < 0, 10, "[CLIENT]:Error at printing server response!")
        CHECK(fflush(stdout) != 0, 5, "[CLIENT]:Error at fflush")
    }
    closeFIFOs();   

    return 0;
}