/*
    @filename: server.c
    @authors: Sriram Sridhar, Venkata Santosh Yashwanth Kukkala
*/
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#define _ATFILE_SOURCE
#define AT_STATX_SYNC_AS_STAT 0x0000
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ftw.h>
#include <libgen.h>
#include <fcntl.h>
#include <gnu/libc-version.h>
#include <time.h>

// Counters, struct and other variables which are required inside nftw are declared global
int client_fd;
int opt = 0;
typedef struct
{
    long int datetime;
    char *filedirname;
} filesst;

struct statx *statxbuf;

filesst *filelist;
int filelistcount = 1;
char *filename_ext;
int fz_lb = 0;
int fz_ub = 0;
int fz_count = 0;
char *response;
int length = 1;
char *ext[3];
long int seconds;
int distrocode = 0;
struct tm tms;
int count = 0;

// Check the distro and assign a value, since many distro does not support file creation date, this is to make sure server uses
// file change date instead
void assigndistrocode()
{
    int fd;
    char distro[100];
    char *name = "NAME=";
    char *current;

    fd = open("/etc/os-release", O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open /etc/os-release");
    }

    read(fd, distro, 100);
    current = strstr(distro, name);

    if (current != NULL)
    {
        strncpy(distro, current + strlen(name), 100 - (current - distro));
    }
    else
    {
        printf("Failed to find NAME in /etc/os-release\n");
    }

    close(fd);

    if (strstr(distro, "Ubuntu") != NULL)
    {
        distrocode = 1;
        printf("Running server on Ubuntu\n");
    }
    else if (strstr(distro, "Debian GNU/Linux") != NULL)
    {
        distrocode = 2;
        printf("Running server on Debian\n");
    }
    // printf("Could not find distro\n");
    fflush(stdout);
}

// Slice the string delimited by space and send it as array
void sliceargs(char *args, char ***argarr)
{
    *argarr = malloc((strlen(args) + 1) * sizeof(char *));
    char *token = strtok(args, " ");
    int i = 0;
    while (token != NULL)
    {
        (*argarr)[i] = malloc(strlen(token) + 1);
        strcpy((*argarr)[i], token);
        token = strtok(NULL, " ");
        i++;
    }
    (*argarr)[i] = NULL;
}

// Bubble Sort to sort the struct array based on time or alphabetical order
void bubbleSort(filesst *arr, int n)
{
    int i, j;
    for (i = 1; i <= n - 1; i++)
    {
        // Last i elements are already in place
        for (j = 1; j <= n - i - 1; j++)
        {
            if (opt == 1)
            {
                // printf("\ni= %d j= %d", i, j);
                // printf("\narr[j].filedirname: %s arr[j + 1].filedirname: %s", arr[j].filedirname, arr[j + 1].filedirname);
                // fflush(stdout);
                // printf("\nstrcasecmp(arr[j].filedirname, arr[j + 1].filedirname): %d", strcasecmp(arr[j].filedirname, arr[j + 1].filedirname));
                // fflush(stdout);
                if (strcasecmp(arr[j].filedirname, arr[j + 1].filedirname) > 0)
                {
                    // Swap datetime
                    long int tempTime = arr[j].datetime;
                    arr[j].datetime = arr[j + 1].datetime;
                    arr[j + 1].datetime = tempTime;

                    // Swap filedirname
                    char *tempName = malloc((strlen(arr[j].filedirname) + 1) * sizeof(char));
                    strcpy(tempName, arr[j].filedirname);

                    arr[j].filedirname = realloc(arr[j].filedirname, (strlen(arr[j + 1].filedirname) + 1) * sizeof(char));
                    strcpy(arr[j].filedirname, arr[j + 1].filedirname);

                    arr[j + 1].filedirname = realloc(arr[j + 1].filedirname, (strlen(tempName) + 1) * sizeof(char));
                    strcpy(arr[j + 1].filedirname, tempName);

                    free(tempName);
                }
            }
            else if (opt == 2)
            {
                if (arr[j].datetime > arr[j + 1].datetime)
                {
                    // Swap datetime
                    long int tempTime = arr[j].datetime;
                    arr[j].datetime = arr[j + 1].datetime;
                    arr[j + 1].datetime = tempTime;

                    // Swap filedirname
                    char *tempName = malloc((strlen(arr[j].filedirname) + 1) * sizeof(char));
                    strcpy(tempName, arr[j].filedirname);

                    arr[j].filedirname = realloc(arr[j].filedirname, (strlen(arr[j + 1].filedirname) + 1) * sizeof(char));
                    strcpy(arr[j].filedirname, arr[j + 1].filedirname);

                    arr[j + 1].filedirname = realloc(arr[j + 1].filedirname, (strlen(tempName) + 1) * sizeof(char));
                    strcpy(arr[j + 1].filedirname, tempName);

                    free(tempName);
                }
            }
        }
    }
}
int nftwfunc(const char *filename, const struct stat *statptr, int fileflags, struct FTW *pfwt)
{
    int val = 1;

    if (opt == 1 || opt == 2)
    {
        // If "dirlist -a" and "dirlist -t", creat
        if (fileflags == FTW_D)
        {
            if (!((strstr(filename, "/.") != NULL) || (strstr(filename, "snap") != NULL) || (strstr(filename, "/workspace/") != NULL)))
            {
                filelist = realloc(filelist, (filelistcount + 1) * sizeof(filesst));
                // If Ubunutu, use statx and populate file creation date else use file change date
                if (distrocode == 1)
                {
                    statxbuf = malloc(sizeof(struct statx));
                    if (statx(AT_FDCWD, filename, AT_STATX_SYNC_AS_STAT, STATX_ALL, statxbuf) == 0)
                    {
                        filelist[filelistcount].datetime = statxbuf->stx_btime.tv_sec;
                    }
                    else
                    {
                        printf("Error populating statxbuf\n");
                        return 1;
                    }
                    free(statxbuf);
                }
                else
                {
                    filelist[filelistcount].datetime = statptr->st_ctim.tv_sec;
                }

                filelist[filelistcount].filedirname = malloc(strlen(filename) * sizeof(char));
                // strcpy(filelist[filelistcount].filedirname, filename);
                filelist[filelistcount].filedirname = strdup(filename);

                filelistcount++;
            }
        }
    }
    else if (opt == 3)
    {
        // Copy file name, size, bytes from stat and filecreation time from statx or file change date from stat to a string
        if (fileflags == FTW_F)
        {
            val = strcmp((char *)filename + pfwt->base, filename_ext);
            // printf("%s\n",filename);
            if (val == 0)
            {
                // printf("\nfilename: %s", filename);

                response = malloc(255 * sizeof(char));
                strcpy(response, filename);
                strcat(response, " Size: ");
                char *fz = malloc(5 * sizeof(char));
                sprintf(fz, "%ld", statptr->st_size);
                strcat(response, fz);
                strcat(response, "bytes ");
                statxbuf = malloc(sizeof(struct statx));
                long int birthtime = 0;
                if (distrocode == 1)
                {
                    if (statx(AT_FDCWD, filename, AT_STATX_SYNC_AS_STAT, STATX_ALL, statxbuf) == 0)
                    {
                        birthtime = statxbuf->stx_btime.tv_sec;
                    }
                }
                else
                {
                    birthtime = statptr->st_ctim.tv_sec;
                }
                time_t btime = (time_t)birthtime;
                struct tm *ptm = malloc(1 * sizeof(struct tm));
                ptm = localtime(&btime);
                char *time = malloc(sizeof(char) * 20);
                sprintf(time, "%s %d:%d %d-%d-%d", "Time: ", ptm->tm_hour, ptm->tm_min, ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900);
                strcat(response, time);
                // printf("\ncrossed time concatenation");
                // strcat(response, "Time: ");
                // strcat(response, ptm->tm_hour);
                // strcat(response, ":");
                // strcat(response, ptm->tm_min);
                // strcat(response, " ");
                // strcat(response, ptm->tm_mday);
                // strcat(response, "-");
                // strcat(response, ptm->tm_mon);
                // strcat(response, "-");
                // strcat(response, ptm->tm_year);
                char *permission = malloc(11 * sizeof(char));
                sprintf(permission, "%c%c%c%c%c%c%c%c%c%c",
                        (S_ISDIR(statptr->st_mode)) ? 'd' : '-',
                        (statptr->st_mode & S_IRUSR) ? 'r' : '-',
                        (statptr->st_mode & S_IWUSR) ? 'w' : '-',
                        (statptr->st_mode & S_IXUSR) ? 'x' : '-',
                        (statptr->st_mode & S_IRGRP) ? 'r' : '-',
                        (statptr->st_mode & S_IWGRP) ? 'w' : '-',
                        (statptr->st_mode & S_IXGRP) ? 'x' : '-',
                        (statptr->st_mode & S_IROTH) ? 'r' : '-',
                        (statptr->st_mode & S_IWOTH) ? 'w' : '-',
                        (statptr->st_mode & S_IXOTH) ? 'x' : '-');
                strcat(response, " Permission: ");
                strcat(response, permission);
                // printf("\n crossed priv concatenation");
                // free(ptm);
                // free(time);

                free(statxbuf);
                free(fz);

                return 1;
            }
        }
    }
    else if (opt == 4)
    {
        // If the file is within the byte size bound, append it to the tar
        if (fileflags == FTW_F &&
            (statptr->st_size >= fz_lb && statptr->st_size <= fz_ub))
        {
            if (((strstr(filename, "/.") != NULL) || (strstr(filename, "snap") != NULL) || (strstr(filename, "/workspace/") != NULL)))
            {
                return 0;
            }
            char *filepath = malloc(strlen(filename) - strlen(filename + pfwt->base) + 1);
            memset(filepath, '\0', (strlen(filename) - strlen(filename + pfwt->base) + 1));
            strncpy(filepath, filename, pfwt->base);
            fz_count++;
            int t = fork();

            if (t == 0)
            {
                // Allocate storage for file path(not file name) and copy the file path

                if (execlp("tar", "tar", "-rf", "tempserver.tar.gz", "-C", filepath, (char *)filename + pfwt->base, NULL) < 0)
                {
                    perror("Error creating or appending to tar");
                    fz_count--;
                    exit(0);
                }
            }
            else
            {
                wait(NULL);
                free(filepath);
            }
        }
    }
    else if (opt == 5)
    {
        // If the file extension is same as the extension given by the client, append it to the tar
        if (fileflags != FTW_F ||
            (strstr(filename, "/.") != NULL) ||
            (strstr(filename, "snap") != NULL) ||
            (strstr(filename, "/workspace/") != NULL))
        {
            return 0;
        }
        int cont = 0;

        char *extf = strrchr(filename, '.');
        if (extf != NULL)
        {
            extf++;

            fflush(stdout);
            for (int i = 0; i < length - 1; i++)
            {
                // printf("\nextf: %s ext[%d]: %s", extf, i, ext[i]);
                fflush(stdout);
                val = strcmp(extf, ext[i]);
                if (val == 0)
                {
                    cont += 1;
                }
            }
            if (cont < 1)
            {
                return 0;
            }
            char *filepath = malloc((strlen(filename) - strlen(filename + pfwt->base) + 1) * sizeof(char));
            memset(filepath, '\0', (strlen(filename) - strlen(filename + pfwt->base) + 1));
            strncpy(filepath, filename, pfwt->base);
            fz_count++;
            int t = fork();

            if (t == 0)
            {
                // printf("\nfilepath: %s", filepath);
                // fflush(stdout);

                if (execlp("tar", "tar", "-rf", "tempserver.tar.gz", "-C", filepath, (char *)filename + pfwt->base, NULL) < 0)
                {
                    perror("Error creating or appending to tar");
                    fz_count--;
                    exit(0);
                }
            }
            else
            {
                wait(NULL);
                free(filepath);
            }
        }
        else
        {
            return 0;
        }
    }
    else if (opt == 6 || opt == 7)
    {
        if (fileflags != FTW_F ||
            (strstr(filename, "/.") != NULL) ||
            (strstr(filename, "snap") != NULL) ||
            (strstr(filename, "/workspace/") != NULL))
        {
            return 0;
        }
        long int temp = 0;
        // Get file creation date or file change date based on distro
        if (distrocode == 1)
        {

            statxbuf = malloc(sizeof(struct statx));
            if (statx(AT_FDCWD, filename, AT_STATX_SYNC_AS_STAT, STATX_ALL, statxbuf) != 0)
            {
                printf("Error populating statxbuf\n");
                return 0;
            }
            temp = statxbuf->stx_btime.tv_sec;
            free(statxbuf);
        }
        else
        {
            temp = statptr->st_ctim.tv_sec;
        }
        char *filepath = malloc((strlen(filename) - strlen(filename + pfwt->base) + 1) * sizeof(char));
        memset(filepath, '\0', (strlen(filename) - strlen(filename + pfwt->base) + 1));
        strncpy(filepath, filename, pfwt->base);
        fz_count++;
        // Append it into tar based on the option, whether the file date should be lesser or greater than the given date
        if (opt == 6)
        {
            if (!(temp <= seconds))
            {
                return 0;
            }


            // Uncomment this if excelp is not working and comment out fork part
            // printf("\n%ld %ld %s", temp, seconds, filename);
            // fflush(stdout);
            // char command[500];
            // char *bname = (char *)filename + pfwt->base;
            // printf("bname: %s, filepath: %s\n", bname, filepath);
            // fflush(stdout);
            // if (fz_count == 0)
            //     sprintf(command, "tar -C %s -cf tempserver.tar.gz %s", filepath, bname);
            // else
            //     sprintf(command, "tar -C %s -rf tempserver.tar.gz %s", filepath, bname);
            // system(command);


            int t = fork();

            if (t == 0)
            {

                if (execlp("tar", "tar", "-rf", "tempserver.tar.gz", "-C", filepath, (char *)filename + pfwt->base, NULL) < 0)
                {
                    perror("Error creating or appending to tar");
                    fz_count--;
                    exit(0);
                }
            }
            else
            {
            wait(NULL);
            free(filepath);
            }
        }
        else if (opt == 7)
        {

            if (!(temp >= seconds))
            {
                return 0;
            }


            // Uncomment this if excelp is not working and comment out fork part
            // printf("\n%ld %ld %s ---- ", temp, seconds, filename);
            // fflush(stdout);
            // char command[500];
            // char *bname = (char *)filename + pfwt->base;
            // printf("bname: %s, filepath: %s\n", bname, filepath);
            // fflush(stdout);
            // if (fz_count == 0)
            //     sprintf(command, "tar -C %s -cf tempserver.tar.gz %s", filepath, bname);
            // else
            //     sprintf(command, "tar -C %s -rf tempserver.tar.gz %s", filepath, bname);
            // system(command);
            // count += 1;
            // printf("Done here: %d\n\n", count);
            // fflush(stdout);


            int t = fork();

            if (t == 0)
            {
                // printf("\nfilepath: %s", filepath);
                // fflush(stdout);

                if (execlp("tar", "tar", "-rf", "tempserver.tar.gz", "-C", filepath, (char *)filename + pfwt->base, NULL) < 0)
                {
                    perror("Error creating or appending to tar");
                    fz_count--;
                    exit(0);
                }
            }
            else
            {
                wait(NULL);
                free(filepath);
            }
        }
    }

    fflush(stdout);
    return 0;
}

// Function to remove new line character
void remove_newline(char *str)
{
    char *p;
    if ((p = strchr(str, '\n')) != NULL)
        *p = '\0';
}
// Function to send the file to client
int send_file(int new_sock, char *filename, int fcount)
{
    // printf("\nfcount: %d", fcount);
    // fflush(stdout);
    if (fcount < 1)
    {
        long int temp = 0;
        int sendstat = send(new_sock, &temp, sizeof(long), 0);
        return -1;
    }
    int n;
    FILE *fp;
    char buffer[512];

    // fp = fopen("tempserver.tar.gz", "rb");
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        return 2;
    }

    // send byte size of the file first
    struct stat filestat;
    // stat("tempserver.tar.gz", &filestat);
    stat(filename, &filestat);
    long int x = filestat.st_size;
    // printf("filesize: %d", (int)x );
    // fflush(stdout);
    if (send(new_sock, &x, sizeof(long), 0) <= 0)
    {
        printf("\nError sending byte length to client");
        fflush(stdout);
        return -1;
    }
    int bytes_sent = 0;

    // read the contents of the file and send to client in a loop
    while ((n = fread(buffer, sizeof(char), 512, fp)) > 0)
    {
        if (send(new_sock, buffer, n, 0) == -1)
        {
            printf("\nError sending the file");
            return -1;
        }
        bzero(buffer, 512);
        bytes_sent += n;
        if (bytes_sent >= filestat.st_size)
        {
            bytes_sent = 0;
            x = 0;
            // remove("tempserver.tar.gz");
            remove(filename);
            // printf("\nbytes sent: %d", bytes_sent);
            break;
        }
    }
    
    // close file
    fclose(fp);
    
    // printf("\n closing file and exiting");
    // fflush(stdout);
    return 1;
}
void performop(char ***argarr)
{
    // printf("\nentered performop");
    int flags = FTW_PHYS;

    if (strcmp((*argarr)[0], "dirlist") == 0)
    {
        // Get array of struct from nftw and sort it using bubble sort based on name or date

        opt = 1;
        if (strcmp((*argarr)[1], "-t\n") == 0)
        {
            opt = 2;
        }

        filelist = malloc(1 * sizeof(filesst));
        nftw(getenv("HOME"), nftwfunc, 64, flags);
        // printf("\nfilelistcount: %d", filelistcount);
        fflush(stdout);

        bubbleSort(filelist, filelistcount);
        // Iterate through the array and put it into the string and send it to client
        for (int i = 1; i < filelistcount; i++)
        {
            // printf("\ntime: %ld filename: %s ", filelist[i].datetime, filelist[i].filedirname);
            fflush(stdout);
            if (i == 1)
            {
                response = malloc(strlen(filelist[i].filedirname) * sizeof(char));
                memset(response, '\0', strlen(filelist[i].filedirname));
                strcat(response, filelist[i].filedirname);
                strcat(response, "\n");
                free(filelist[i].filedirname);
                continue;
            }
            char *temp = realloc(response, (strlen(response) + strlen(filelist[i].filedirname) + 2) * sizeof(char)); // Use a temporary pointer for realloc
            if (temp == NULL)
            {
                // Handle error
                printf("Error reallocating memory!\n");
                free(response);
                return;
            }
            response = temp;
            // void *pt = realloc(response, ((strlen(response) + strlen(filelist[i].filedirname) + 1) * sizeof(char)));
            strcat(response, filelist[i].filedirname);
            strcat(response, "\n");
            free(filelist[i].filedirname);
        }
        filelistcount = 1;
        free(filelist);
        // printf("\ncontents in response: %s \nsize of response: %ld",response, strlen(response));
        long int len = strlen(response);
        write(client_fd, &len, sizeof(long));
        write(client_fd, response, strlen(response));
        // printf("\nSending length: %ld", strlen(response));
        free(response);
        // }
    }
    else if (strcmp((*argarr)[0], "w24fn") == 0)
    {
        // Store the filename in global variable and invoke the nftw. Send the response to client
        filename_ext = malloc(strlen((*argarr)[1]) * sizeof(char));
        strcpy(filename_ext, (*argarr)[1]);
        remove_newline(filename_ext);
        opt = 3;
        nftw(getenv("HOME"), nftwfunc, 64, flags);
        if (response != NULL)
        {
            long int len = strlen(response);
            write(client_fd, &len, sizeof(long));
            write(client_fd, response, strlen(response));
            free(response);
        }

        // printf("\n%s\n", filename_ext);
        free(filename_ext);
    }
    else if (strcmp((*argarr)[0], "w24fz") == 0)
    {
        // Store the file sizes and invoke nftw

        // printf("\nentering into w24fz");
        // fflush(stdout);
        opt = 4;
        fz_lb = atoi((*argarr)[1]);
        remove_newline((*argarr)[2]);
        fz_ub = atoi((*argarr)[2]);
        // printf("\nfz_lb=%d fz_ub=%d", fz_lb, fz_ub);

        nftw(getenv("HOME"), nftwfunc, 64, flags);
        // Send the tar file to client
        if (send_file(client_fd, "tempserver.tar.gz", fz_count) < 0)
        {
            printf("\nSending file unsuccessful");
            // return 0;
        }
        fz_lb = 0;
        fz_ub = 0;
        fz_count = 0;

        // write_file(client_fd);
    }
    else if (strcmp((*argarr)[0], "w24ft") == 0)
    {

        opt = 5;
        // printf("\nentering into w24ft");
        // fflush(stdout);

        // Store the list of extensions in an global variable array for nftw
        while ((*argarr)[length] != NULL)
        {
            ext[length - 1] = malloc(strlen((*argarr)[length]) * sizeof(char));
            strcpy(ext[length - 1], (*argarr)[length]);
            length++;
        }
        // Remove \n
        remove_newline(ext[length - 2]);

        nftw(getenv("HOME"), nftwfunc, 64, flags);
        // nftw(getenv("HOME"), nftwfunc, 64, flags);
        // After nftw, send the tar to client
        if (send_file(client_fd, "tempserver.tar.gz", fz_count) < 0)
        {
            printf("\nSending file unsuccessful");
            // return 0;
        }

        for (int i = 0; i < length; i++)
        {
            free(ext[i]);
            // printf("\next[%d]: %s",i,ext[i]);
        }
        fz_count = 0;
        length = 1;
    }
    else if ((strcmp((*argarr)[0], "w24fdb") == 0) || strcmp((*argarr)[0], "w24fda") == 0)
    {
        // Parse the date, convert it to secs starting from January 1 1970 and store it in global variable for nftw
        // Invoke nftw and send the tar file if it creates one
        opt = 6;
        fz_count = 0;
        seconds = 0;
        if (strcmp((*argarr)[0], "w24fda") == 0)
        {
            opt = 7;
        }
        remove_newline((*argarr)[1]);
        
        strptime((*argarr)[1], "%Y-%m-%d", &tms);
        seconds = mktime(&tms);
        // free(tms);/
        // printf("ONES\n\n");
        // fflush(stdout);

        nftw(getenv("HOME"), nftwfunc, 64, flags);

        // printf("TWOSS\n\n");
        // fflush(stdout);
        count = 0;
        if (send_file(client_fd, "tempserver.tar.gz", fz_count) < 0)
        {
            printf("\nSending file unsuccessful");
            // return 0;
        }
        fz_count = 0;
    }

    opt = 0;
    // fflush(stdout);
}

// Route receiving connection to mirror by sending port number
void route_connection(int port)
{
    if (send(client_fd, &port, sizeof(int), 0) < 0)
    {
        perror("TCP Server-Connection Acknowledgement Send failed");
        exit(EXIT_FAILURE);
    }
    printf("\nSent port number %d to client\n", port);
}

// function to process client request
void crequest()
{
    while (1)
    {
        char *buffer = malloc(256 * sizeof(char));
        memset(buffer, '\0', 256);
        int n = read(client_fd, buffer, 255);
        if (n < 0)
            perror("ERROR reading from socket");

        char **argarr;
        sliceargs(buffer, &argarr);
        if(strcmp(argarr[0],"quitc\n")==0){
            printf("\nClient exiting\n");
            fflush(stdout);
        }
        performop(&argarr);

        free(buffer);
        free(argarr);
    }
}

void main(int argc, char *argv[])
{
    // Assign distro type
    assigndistrocode();
    // Variables for file descriptors for sockets and
    int socket_fd;
    int servercounter = 0;
    int mirror1_port = 3600;
    int mirror2_port = 3700;
    struct sockaddr_in server_addr;
    // socklen_t address_len = sizeof(server_addr);

    // Establish socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(3200);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    int optname = 1;

    // Bind and listen
    if ((socket_fd < 0) ||
        (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optname, sizeof(optname)) < 0) ||
        (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) ||
        (listen(socket_fd, 5) < 0))
    {
        perror("\nSocket Deployment failed");
        return;
    }

    while (1)
    {
        // printf("\nwhile loop\n");
        // fflush(stdout);
        struct sockaddr_in client_addr;
        socklen_t address_len = sizeof(client_addr);

        printf("Server waiting for new client\n");
        // printf("\nbefore accept\n");

        // Accept new connection
        client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &address_len);
        if (client_fd < 0)
        {
            perror("ERROR on accept");
            break;
        }

        // Increment the counter, if the counter exceeds 3, send it to mirror1, if the counter exceeds 6 send to mirror2
        // if the counter exceeds 9 keep alternating between 3 servers
        servercounter++;
        printf("\nServercounter: %d\n", servercounter);

        if (servercounter > 3 && servercounter <= 6)
        {
            printf("\nServer busy, routing to mirror1");
            route_connection(mirror1_port);
            close(client_fd);
            continue;
            // Send
        }
        else if (servercounter > 6 && servercounter <= 9)
        {
            printf("\nServer busy, routing to mirror2");
            route_connection(mirror2_port);
            close(client_fd);
            continue;
        }
        else if (servercounter > 9)
        {
            switch (servercounter % 3)
            {
            case 1:
                // printf("\nServer busy, routing to serverw24");
                break;
            case 2:
                printf("\nServer busy, routing to mirror1");
                route_connection(mirror1_port);
                close(client_fd);
                continue;
                break;
            case 0:
                printf("\nServer busy, routing to mirror2");
                route_connection(mirror2_port);
                close(client_fd);
                continue;
                break;
            default:
                break;
            }
        }
        // send(client_fd, 0 , sizeof(int), 0);
        route_connection(0);

        printf("Client connected with Server24\n");

        // Fork for every new client and perform operation
        int d = fork();
        if (d == 0)
        {
            crequest();
        }

        close(client_fd);
    }
}
