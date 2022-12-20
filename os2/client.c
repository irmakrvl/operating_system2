#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 3205
#define BUFFER_SIZE 1024

int socketFd;

void die(char *info)
{
    perror(info);
    close(socketFd);
    exit(EXIT_FAILURE);
}

int main()
{
    //create socket variables
    struct sockaddr_in server_address;
    socketFd = sizeof(server_address);
    char msgBuffer[BUFFER_SIZE],chatBuffer[BUFFER_SIZE],chatMsg[BUFFER_SIZE];

    memset((char *) &server_address, 0, sizeof(server_address));
    memset(msgBuffer, 0, sizeof msgBuffer);
    //define address info
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);

    if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) <0 )
        die("Socket creation failed");

    if (connect(socketFd, (struct sockaddr *) &server_address,sizeof(server_address)) < 0)
        die("Server connection failed");

    printf("Server connected.\n");

    int numBytesRead;
    fd_set clientFds;

    while(1)
    {
        fflush(stdout);
        FD_ZERO(&clientFds);
        FD_SET(socketFd, &clientFds);
        FD_SET(0, &clientFds);
        //wait for an available fd
        if(select(FD_SETSIZE, &clientFds, NULL, NULL, NULL) != -1)
        {
            for(int fd = 0; fd < FD_SETSIZE; fd++)
            {
                if(FD_ISSET(fd, &clientFds))
                {
                    if(fd == socketFd) //receive data from server
                    {
                        int numBytesRead = read(socketFd, msgBuffer, BUFFER_SIZE - 1);
                        msgBuffer[numBytesRead] = '\0';
                        printf("%s", msgBuffer);
                        memset(&msgBuffer, 0, sizeof(msgBuffer));
                    }
                    else if(fd == 0) //if exit entered
                    {
                        fgets(chatBuffer, BUFFER_SIZE - 1, stdin);
                        if(strcmp(chatBuffer, "-exit") == 0)
                        {
                            if(write(socketFd, "-exit", BUFFER_SIZE - 1) < 0)
                                die("Write failed");
                            break;

                        }
                        else
                        {
                            //send msg to server
                            memset(chatMsg, 0, BUFFER_SIZE);
                            strcpy(chatMsg, chatBuffer);
                            if(write(socketFd, chatMsg, BUFFER_SIZE - 1) <0)
                                die("Write failed");

                            memset(&chatBuffer, 0, sizeof(chatBuffer));
                        }
                    }
                }
            }
        }
    }

    close(socketFd);
    exit(EXIT_SUCCESS);

}

