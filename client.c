/*
    @filename: client.c
    @authors: Venkata Santosh Yashwanth Kukkala, Sriram Sridhar
*/

#include <sys/socket.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <utime.h>
#include <sys/signal.h>
#include <fcntl.h>

int socket_kill = 0; // socket termination counter

// funtion for tokenizing the input from user
void sliceargs(char *args, char ***argarr)
{
    *argarr = malloc((strlen(args) + 1) * sizeof(char *));
    char *token = strtok(args, " "); // tokenizing based on space
    int i = 0;
    while (token != NULL)
    {
        (*argarr)[i] = malloc(strlen(token) + 1);
        strcpy((*argarr)[i], token); // Copies the content of token
        token = strtok(NULL, " ");   // Continues tokenizing to get the next token
        i++;
    }
    char *p;
    if ((p = strchr((*argarr)[i - 1], '\n')) != NULL) // Checks if there is a newline character at the last.
        *p = '\0';                                    // Replaces the newline character ('\n') with a null terminator ('\0')
    (*argarr)[i] = NULL;
}

// function to check if year entered is a leap year
int leapyear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// function to validate the date entered...
int validate_date(int year, int month, int day)
{
    if (year < 1970 || year == 0 || year > 2025) // checking year
    {
        printf("Enter year greater than 1970\n");
        return 0;
    }
    if (month > 12 || month == 0) // checking month
    {
        printf("Enter valid month\n");
        return 0;
    }
    if (day == 0 || day > 31) // checking day
    {
        printf("Enter valid day between 1 and 31\n");
        return 0;
    }
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && leapyear(year)) // checking for leap year
    {
        days_in_month[2] = 29;
    }
    if (day > days_in_month[month])
    {
        printf("Error: Invalid day for the given month\n");
        return 0;
    }

    return 1;
}

// function to parse date input, validate it and send it to server
int date2Str(char *input)
{
    struct tm dateInfo = {0};
    int y, m, d;
    int result = sscanf(input, "%d-%d-%d", &y, &m, &d); // parse formatted input from the string input into three integer variables
    if (result != 3)
    {
        printf("Invalid date format. Please use YYYY-MM-DD.\n");
        return -1;
    }

    // Set daylight saving time flag to unspecified
    dateInfo.tm_isdst = -1;

    if (!validate_date(y, m, d))
    {
        return -1;
    }
    return 1;
}

// Function to check syntax of user command entered
int syntax_check(char ***cmdenter)
{
    // printf("(*cmdenter)[0] %s", (*cmdenter)[0]);
    if (strcmp((*cmdenter)[0], "dirlist") == 0) // dirlist
    {
        if ((strcmp((*cmdenter)[1], "-a") == 0) && ((*cmdenter)[2] == NULL))
        {
            return 1; // Valid dirlist -a command
        }
        if ((strcmp((*cmdenter)[1], "-t") == 0) && ((*cmdenter)[2] == NULL))
        {
            return 1; // Valid dirlist -t command
        }
        return -1; // Invalid dirlist command
    }
    else if (strcmp((*cmdenter)[0], "w24fn") == 0) // w24fn
    {
        if (((*cmdenter)[1] != NULL) && ((*cmdenter)[2] == NULL))
        {
            return 1; // Valid w24fn command
        }
        // printf("\nUsage: w24fn <filename>\n");
        return -1; // Invalid w24fn command
    }
    else if (strcmp((*cmdenter)[0], "w24fz") == 0) // w24fz
    {
        if (((*cmdenter)[1] != NULL) &&
            ((*cmdenter)[2] != NULL) &&
            ((*cmdenter)[3] == NULL))
        {
            int fz_lb = atoi((*cmdenter)[1]);
            // remove_newline((*argarr)[2]);
            int fz_ub = atoi((*cmdenter)[2]);
            if (fz_ub >= fz_lb)
            {
                return 2; // Valid w24fz command
            }
        }
        return -1;
    }
    else if (strcmp((*cmdenter)[0], "w24ft") == 0) // w24ft
    {
        int len = 1;
        while ((*cmdenter)[len] != NULL)
        {
            len++;
        }
        // printf("\nlen: %d",len);
        if (len < 5)
        {
            return 2; // Valid w24ft command
        }
        return -1;
    }
    else if (strcmp((*cmdenter)[0], "w24fdb") == 0) // w24fdb
    {
        // time_t date1 = str2date(store_cmdenter[0]);
        if ((date2Str((*cmdenter)[1]) == -1) || (*cmdenter)[2] != NULL)
        {
            // printf("Invalid date \n");
            return -1;
        }
        return 2; // Valid w24fdb command
    }
    else if (strcmp((*cmdenter)[0], "w24fda") == 0) // w24fda
    {
        // time_t date1 = str2date(store_cmdenter[0]);
        if ((date2Str((*cmdenter)[1]) == -1) || (*cmdenter)[2] != NULL)
        {
            // printf("Invalid date \n");
            return -1;
        }
        return 2; // Valid w24fdb command
    }
    else if (strcmp((*cmdenter)[0], "quitc") == 0) // quitc
    {
        return 6; // Valid quit command
    }
    else
    {
        printf("Command not supported!\n");
        return -1; // Invalid command
    }
}

// function to recieve file's data over a socket (sock), write it to a file named recv_file.tar.gz
void write_file(int sock)
{
    // recieve the byte size of the file
    long int bytes_to_recv = 0;
    int recstat = recv(sock, &bytes_to_recv, sizeof(long), 0); // receives data from sock into the bytes_to_recv variable
    // printf("\nbytes_to_recv: %ld\n", bytes_to_recv);
    fflush(stdout);
    if (bytes_to_recv < 1) // error condition
    {
        printf("\nNo files found or error processing tar\n");
        return;
    }
    int n; // variable to store no. of bytes
    FILE *fp;
    char *filename = "/home/gideon/temp.tar.gz"; // recieved data file
    char buffer[512];

    fp = fopen(filename, "wb"); // opening file in binary write mode
    if (fp == NULL)
    {
        perror(" - Error in opening file. Exiting . . .\n");
        exit(1); // exit if error
    }

    int bytes_received = 0;
    //  while Loop recieving the file from server

    int c = 0;
    while ((n = recv(sock, buffer, 512, 0)) > 0)
    {
        // if (c == 0)

        if (fwrite(buffer, sizeof(char), n, fp) != n) // writing files from buffer to file pointed by fp
        {
            perror(" - Error in writing file. Exiting . . .\n");
            exit(1); // exit if error
        }
        bzero(buffer, 512);                  // clears the buffer
        bytes_received += n;                 // updation of bytes recieved
        if (bytes_received >= bytes_to_recv) // Checks if all expected bytes (bytes_to_recv) have been received.
        {
            printf("\nbytes recieved: %d\n", bytes_received);
            bytes_received = 0;
            bytes_to_recv = 0;
            break;
        }
        c++;
    }
    // close file
    fclose(fp);
    // printf("\nclosing the file and returning\n");
}

// function receives a message size from the server, store it, receives the message content and prints it.
void print_output(int serverfd)
{

    long int bytes_to_recv = 0;
    int recstat; // recv(serverfd, &bytes_to_recv, sizeof(long), 0);
    // recieve data through socket
    if ((recstat = recv(serverfd, &bytes_to_recv, sizeof(long), 0)) < 0)
    {
        printf("\nError recieving size\n");
        return;
    }

    char *msg_from_server = malloc((bytes_to_recv + 1) * sizeof(char)); // allocating memory
    memset(msg_from_server, '\0', (bytes_to_recv + 1));                 // initializing memory block to '/0'
    if ((recstat = recv(serverfd, msg_from_server, bytes_to_recv, 0)) < 0)
    {
        printf("\nError recieving content from server\n");
        return;
    }
    printf("\n%s\n", msg_from_server);
    free(msg_from_server); // releasing memory....
    return;
}

// main function
int main(int argc, char *argv[])
{
    int server_fd = 0;
    struct sockaddr_in servaddress; // struct for storing socket related addresses
    servaddress.sin_family = AF_INET;
    servaddress.sin_port = htons(3200); // server port

    // server_fd = socket(AF_INET, SOCK_STREAM, 0); // create client socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
        inet_pton(AF_INET, "127.0.0.1", &servaddress.sin_addr) <= 0 ||
        connect(server_fd, (struct sockaddr *)&servaddress, sizeof(servaddress))) // performing connect through socket server_fd
    {
        printf("\nFailed to connect to server");
        printf("\nServer could be offline");
        close(server_fd); // closing socket
        exit(1);          // exit failure
    }

    // Recieve integer value from client. port=0 -> connected to server. port>0 -> connect to mirror
    int port = 0;
    recv(server_fd, &port, sizeof(int), 0);
    printf("\nValue received from server: %d\n", port);
    if (port != 0) // connection logic to mirror 1 nad mirror2
    {
        close(server_fd);
        // configure server address
        servaddress.sin_family = AF_INET;
        servaddress.sin_port = htons(port); // mirror port
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
            inet_pton(AF_INET, "127.0.0.1", &servaddress.sin_addr) <= 0 ||
            connect(server_fd, (struct sockaddr *)&servaddress, sizeof(servaddress))) // performing connect through socket server_fd
        {
            printf("\nFailed to connect to mirror");
            printf("\nMirror could be offline");
            close(server_fd);
            exit(1);
        }
        printf("\nMirror connection successful");
    }
    else // Server connection
    {
        printf("Server Connection Successful! \n");
    }

    // runs in infinite loop and waits for user input
    while (1)
    {
        printf("\nclientw24$ ");
        char *usr_cmd = malloc(1024 * sizeof(char)); // user input
        char *frmsrvr = malloc(1024 * sizeof(char)); // variable used for processing user input and send it to server
        memset(usr_cmd, 0, sizeof(usr_cmd));         // sets all the elements stored in usr_cmd to 0
        if (fgets(usr_cmd, 1024, stdin) == NULL)
        {
            printf("Error reading\n");
            continue;
            // break;
        }
        remove("/home/gideon/temp.tar.gz");
        memset(frmsrvr, 0, sizeof(frmsrvr)); // sets all the elements stored in frmsrvr to 0
        strcpy(frmsrvr, usr_cmd);
        // frmserver1[strcspn(frmserver1, "\n")] = '\0';
        char **argarr;
        sliceargs(usr_cmd, &argarr); // calling tokenizing function

        int c = syntax_check(&argarr); // calling function which will check the syntax of userinput
        if (c == -1)
        {
            printf("\nInvalid command\n");
            continue;
        }

        // send command to server
        // char *respfromserver = malloc(9999999 * sizeof(char));
        if (send(server_fd, frmsrvr, strlen(frmsrvr), 0) < 0)
        {
            printf("\nSend failed");
        }

        free(frmsrvr);
        free(usr_cmd);
        // memset(respfromserver, 0, 9999999);

        if (c == 6) // if quitc has been entered by the user..
        {
            printf("\nExiting");
            // send(server_fd, argarr[0], sizeof(argarr[0]), 0);
            // if (send(server_fd, argarr[0], sizeof(argarr[0]), 0) < 0)
            // {
            //     printf("\nSend failed");
            // }
            close(server_fd);
            exit(0);
        }
        else if (c == 1)
        {
            printf("\nWaiting to read");
            fflush(stdout);
            print_output(server_fd);
            continue;
        }
        else if (c == 2)
        {
            printf("\nWaiting to read");
            fflush(stdout);
            write_file(server_fd);
            continue;
        }

        fflush(stdout);
    }
    close(server_fd); // closing client socket
    return 0;
}
