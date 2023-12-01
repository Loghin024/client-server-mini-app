/*
###########################################################################################################
+++*****************************************************************************************************+++
___________Server Application (server.c) - Simple Documentation____________________________________________    
    
    ->Introduction:
        This program acts as a server that listens for client requests through named pipes (FIFOs). For each command
        received from a client, it creates a new child process to process the command and sends the result back to the
        client for display.

    ->Usage:
    - Compile: gcc server.c -o server
    - Run: ./server

*/

#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <utmp.h>
#include <time.h>

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

char command[COMMAND_BUFFER_SIZE];
char response[RESPONSE_BUFFER_SIZE];
int responseSize;
int isLogged = 0;

char **arguments = NULL;//store user commands

char *process_info[5] = {"Name", "State", "PPid", "Uid", "VmSize"};

bool FIFOexists(const char* pathToFifo)
{
    struct stat st;
    return (stat(pathToFifo, &st) == 0 && S_ISFIFO(st.st_mode));
}

void openFIFOs()
{
    //client server fifo
    if(!FIFOexists(CLIENT_SERVER_FIFO)) //create fifo if it doesn't exists 
        CHECK(-1 == mkfifo(CLIENT_SERVER_FIFO, 0600), 1, "[SERVER]:Error at creating fifo file!\n")
    CHECK(-1 == (fifo_client_server_descriptor = open(CLIENT_SERVER_FIFO, O_RDONLY)), 2, "[CLIENT]:Error at opening fifo file!\n")

    //server client fifo
    if(!FIFOexists(SERVER_CLIENT_FIFO)) //create fifo if it doesn't exists 
        CHECK(-1 == mkfifo(SERVER_CLIENT_FIFO, 0600), 1, "[SERVER]:Error at creating fifo file!\n")
    CHECK(-1 == (fifo_server_client_descriptor = open(SERVER_CLIENT_FIFO, O_WRONLY)), 2, "[CLIENT]:Error at opening fifo file!\n")
}

void closeFIFOs()
{
    CHECK(-1 == close(fifo_client_server_descriptor), 3, "[SERVER]:Error at closing fifo file")
    CHECK(-1 == close(fifo_server_client_descriptor), 3, "[SERVER]:Error at closing fifo file")
}

void read_user_command()
{
    int read_cod = 0;
    int offset = 0;
    while(1)
    {
        char read_ch;
        CHECK(-1 == (read_cod = read(fifo_client_server_descriptor, &read_ch, sizeof(char))), 4, "[SERVER]:Error at reaceiving command from client!")
        response[offset] = read_ch;
        if(read_cod == 0)
        {
            response[offset + 1] = '\0';
            break;
        }

        if(read_ch == '\n')
        {
            response[offset + 1] = '\0';
            break;
        }
        offset++;
    }
}

int read_users(int users_file_descriptor)
{
    int read_cod = 0;
    int offset = 0;
    while(1)
    {
        char read_ch;
        CHECK(-1 == (read_cod = read(users_file_descriptor, &read_ch, sizeof(char))), 4, "[SERVER]:Error at getting valid users for users.txt!")
        response[offset] = read_ch;
        if(read_cod == 0)
        {
            response[offset + 1] = '\0';
            if(offset)
                return offset;
            else
                return -1;
        }

        if(read_ch == '\n')
        {
            response[offset + 1] = '\0';
            return offset;
        }
        offset++;
    }
}

bool validateNumber(char *number)
{
    int numberOfDigits = strlen(number);
    for(int i = 0; i < numberOfDigits-1; ++i) {
        if(!(number[i] >= '0' && number[i] <= '9'))
        {
            return false;
        }
    }
    return true;
}

int checker(char *command)
{
    char copy_command[strlen(command) +1];
    int words_counter = 0;

    //create a copy of the command to get the number of words
    CHECK(NULL == strcpy(copy_command, command), 7, "[SERVER]:Error at strcpy")
    char *token = strtok(copy_command, " ");


    while(token)
    {   
        words_counter++;
        char *newWord = NULL;
        CHECK(NULL == (newWord = malloc(strlen(token) + 1)), 8, "[SERVER]:Error at malloc!")
        CHECK(NULL == strcpy(newWord, token), 7, "[SERVER]:Error at strcpy")

        arguments = (char**)realloc(arguments, words_counter * sizeof(char *));
        arguments[words_counter - 1] = newWord;
        token = strtok(NULL, " ");
    }

    if(!words_counter)
        return -1;

    if(!strcmp(command, "quit\n"))
        return 1;
    else if (!strcmp(command, "logout\n"))
        return 2;
    else if(!strcmp(command, "get-logged-users\n"))
        return 3;
    else
    {
        if(words_counter != 3)
            return -1;

        if(!strcmp(arguments[0], "login") && !strcmp(arguments[1], ":"))
            return 4;
        
        if(!strcmp(arguments[0], "get-proc-info") && !strcmp(arguments[1], ":") && validateNumber(arguments[2]))
        {   
            return 5;
        }
    }

    return -1;
}

void send_response_to_client(char*answer)
{
    // char *answer = "Server can not recognize entered command!\n\0";
    int answer_len = strlen(answer);
    CHECK(-1 == write(fifo_server_client_descriptor, &answer_len, sizeof(int)), 14, "[SERVER]:Error at sending prefix to client!")
    CHECK(-1 == write(fifo_server_client_descriptor, answer, answer_len ), 15, "[SERVER]:Error at sending message to client!")
            
}

void log_user(char *user) {
    int child_pid;
    int p[2];
    CHECK(-1 == pipe(p), 10, "[SERVER]:Error at creating pipe!")
    CHECK(-1 == (child_pid = fork()), 9, "[SERVER]:Error at fork")

    if(child_pid)
    {
        //code executed by parent
        CHECK(-1 == close(p[1]), 11, "[SERVER]:Error at closing writing pipe")
        wait(NULL);
        //get answer from child
        bool child_response = false;
        CHECK(-1 == read(p[0], &child_response, sizeof(bool)), 14, "[Server]:Error at reading from pipe!")
        if(child_response)
            {
                send_response_to_client("Logging finished with result: succes!\n\0");
                isLogged = 1;
            }
        else
        {
            isLogged = 0;
            send_response_to_client("User could not be found!\n\0");
        }
        CHECK(-1 == close(p[0]), 11, "[SERVER]:Error at closing reading pipe!")
    }
    else{
        //code executed by son
        bool found = false;
        CHECK(-1 == close(p[0]), 11, "[SERVER-CHILD]:Error at closing reading pipe")
        int users_file_descriptor;
        CHECK(-1 == (users_file_descriptor = open("users.txt", O_RDONLY)), 12, "[SERVER-CHILD]:Error at opening users.txt file")

        while (read_users(users_file_descriptor) != -1)
        {
           if(!strcmp(arguments[2], response))
           {    
                found = true;
                //send to parent that user was found
                CHECK(-1 == write(p[1], &found, sizeof(bool)), 13, "[SERVER-CHILD]:Error at writing into pipe!")
                break;
           }
        }
        if(!found)
            CHECK(-1 == write(p[1], &found, sizeof(bool)), 13, "[SERVER-CHILD]:Error at writing into pipe!")

        CHECK(-1 == close(p[1]), 11, "[SERVER-CHILD]:Error at closing writing pipe")
        exit(0);//son finished with result succes!
    }
}

int read_from_socket(int socket_descriptor) {
    int offset = 0;
    
    while(1) {
        char read_ch;
        int read_code = 0;
        CHECK(-1 == (read_code = read(socket_descriptor, &read_ch, sizeof(char))), 16, "[SERVER]:Error at reading from socket")
        if (!read_code) {
            response[offset] = '\0';
            return offset;
        }
        response[offset++] = read_ch;
    }
}

void get_logged_users()
{
    int child_pid;
    int socket[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, socket), 14, "[SERVER]:Error at creating socket pair!\n")
    CHECK(-1 == (child_pid = fork()), 9, "[SERVER]:Error at fork")

    if(child_pid)
    {
        //code executed by parent
        CHECK(-1 == close(socket[1]), 15, "[SERVER-PARENT]:Error at closing socket")
        wait(NULL);
        read_from_socket(socket[0]);
        CHECK(-1 == close(socket[0]), 15, "[SERVER-PARENT]:Error at closing socket")
        send_response_to_client(response);
        
    }
    else
    {
        //code executed by child
        CHECK(-1 == close(socket[0]), 15, "[SERVER-PARENT]:Error at closing socket")
        struct utmp * utmp;
        int offset = 0;
        while((utmp = getutent()) != NULL) 
        {
            char user[UT_NAMESIZE];
            strncpy(user,  utmp->ut_user, UT_NAMESIZE);
            char host[UT_HOSTSIZE];
            strncpy(host, utmp->ut_host, UT_HOSTSIZE);
            const long int sec = utmp -> ut_tv.tv_sec;
            char user_info[2048];
            int len = 0;
            CHECK((len = snprintf(user_info, 2048, "username: %s\nhostname for remote login: %s\ntime entry was made: %lds\n", user, host, sec)) < 0, 17, "[SERVER-CHILD]: Error at sprintf(): ");
            int i = 0;
            while(i < len)
            {
                response[offset] = user_info[i];
                offset++;
                i++;
            }
        }

        if(offset && response[offset - 1] == '\n')
            response[offset - 1] = '\0';
        else 
            response[offset] = '\0';

        CHECK(-1 == write(socket[1], response, strlen(response)), 16, "[SERVER-CHILD]:Error at writing in socket!")
        CHECK(-1 == close(socket[1]), 15, "[SERVER-PARENT]:Error at closing socket")
        exit(0);
    }
}

void get_proc_info()
{
    int child_pid;
    int socket[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, socket), 14, "[SERVER]:Error at creating socket pair!\n")
    CHECK(-1 == (child_pid = fork()), 9, "[SERVER]:Error at fork")

    if(child_pid)
    {
        //code executed by parent
        CHECK(-1 == close(socket[1]), 15, "[SERVER-PARENT]:Error at closing socket")
        wait(NULL);
        read_from_socket(socket[0]);
        CHECK(-1 == close(socket[0]), 15, "[SERVER-PARENT]:Error at closing socket")
        send_response_to_client(response);
        
    }
    else
    {
        //code executed by child
        CHECK(-1 == close(socket[0]), 15, "[SERVER-PARENT]:Error at closing socket")

        char wanted_file[32];
        arguments[2][strlen(arguments[2]) - 1] = '\0';
        CHECK(snprintf(wanted_file, 32, "/proc/%s/status", arguments[2]) < 0, 19, "[SERVER-CHILD]:Error at snprintf")
        if(-1 == access(wanted_file, F_OK))
        {
            // send_response_to_client("A process with this pid doesn't exists!");
            strcpy(response, "A process with this pid doesn't exists!");
        }
        else
        {
            int wanted_file_descriptor = 0;
            CHECK(-1 == (wanted_file_descriptor = open(wanted_file, O_RDONLY)), 20, "[SERVER-CHILD]:Error at opening process file!")
            int line_size;
            while ((line_size = read_users(wanted_file_descriptor)) != -1) {
                for (int i = 0; i < 5; ++i) {
                    if (strstr(response, process_info[i])) {
                        CHECK(-1 == write(socket[1], response, strlen(response)), 16, "[SERVER-CHILD]:Error at writing in socket!")
                    }
                }
            }
        }

       // CHECK(-1 == write(socket[1], response, strlen(response)), 16, "[SERVER-CHILD]:Error at writing in socket!")
        CHECK(-1 == close(socket[1]), 15, "[SERVER-PARENT]:Error at closing socket")
        exit(0);
    }
}

int main()
{
    //opening fifo files for communication between client and server
    openFIFOs();

    // running server app 
    while(true)
    {   
        
        read_user_command();    
        //check for valid command
        int checker_response = checker(response);
        if(checker_response == -1)
        {
            // char *answer = "Server can not recognize entered command!\n\0";
            // int answer_len = strlen(answer);
            // CHECK(-1 == write(fifo_server_client_descriptor, &answer_len, sizeof(int)), 14, "[SERVER]:Error at sending prefix to client!")
            // CHECK(-1 == write(fifo_server_client_descriptor, answer, answer_len + 1), 15, "[SERVER]:Error at sending message to client!")
            send_response_to_client("Server can not recognize entered command!\n\0");
            continue;
        }
        //close server
        if(checker_response == 1)
        {
            isLogged = 0;
            exit(0);
        }
        else if(checker_response == 4)
        {
            if(!isLogged)
                log_user(arguments[2]);
            else
                send_response_to_client("An user is already logged!\n\0");
        }
        else if(checker_response == 2)
        {
            if(!isLogged)
                send_response_to_client("You have to be logged in order to log out!\n");
            else
                {
                    isLogged = 0;
                    send_response_to_client("You are logged out!\n");
                }
        }
        else if(checker_response == 3)
        {
            if(!isLogged)
            {
                send_response_to_client("You have to be logged in order to execute get-logged-users command!\n");
            }
            else
            {
                get_logged_users();
            }
        }
        else if(checker_response == 5)
        {
            if(!isLogged)
            {
                send_response_to_client("You have to be logged in order to execute get-proc-info command!\n");
            }
            else
            {
                get_proc_info();
            }
        }

    }

    closeFIFOs();
    return 0;
}