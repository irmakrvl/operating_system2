#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 3205   //The port on which to listen for incoming data
#define BUFFER_SIZE 1024 //Max length of buffer
#define MAX_GROUP_NUMBER 20
#define MAX_CLIENT_PER_GROUP 50

static const int MAXPENDING=10;
//client structure
typedef struct
{
    char phone_number[BUFFER_SIZE];
    char group_name[BUFFER_SIZE];
    int socketFd;
    pthread_mutex_t *clientMutex;
} client;
//group structure
typedef struct
{
    char name[BUFFER_SIZE];
    char pass[BUFFER_SIZE];
    pthread_mutex_t *groupMutex;
    client clients[MAX_CLIENT_PER_GROUP];
    int clientNo;
} group;
//global variables
pthread_mutex_t *globmutex;
group groups[MAX_GROUP_NUMBER];
int groupNo=0;

void die(char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}
//write to connection pipe
void writeMsg(char *msg, int sockfd)
{
    char *msgBuffer[BUFFER_SIZE];

    memset(msgBuffer, 0, sizeof(msgBuffer));
    strcpy(msgBuffer, msg);

    if (write(sockfd, msgBuffer, BUFFER_SIZE-1) < 0)
        die("TCP send failed");
    //simulate distant user
    sleep(1);
}
//convert text to JSON
char* txt2JSON(char *body,char *from,char *group)
{
    char *str;
    strcpy(str, "{'message':{'from':'");
    strcat(str, from);
    strcat(str, "','to':'");
    strcat(str, group);
    strcat(str, "','body':'");
    strcat(str, body);
    return strcat(str, "'}}");
}

void *connection_handler(void *arg)
{
    int sockfd=((int *)arg);
    int numBytesRead;
    char msgBuffer[BUFFER_SIZE];
    bool firstCon=true;

    char *info="*** Wellcome to Messaging server ***\n"
               "Usage:\n"
               "-gcreate phone_number+group_name : Creates a new specified group.\n"
               "-join username/group_name : Enter to the specified username or group name.\n"
               "-exit group_name : Quit from the group that you are in.\n"
               "-send message_body : Send a JSON-formatted message to the group that you are in.\n"
               "-whoami Shows your own username (phone_number) information.\n"
               "-exit Exit the program.\n\n"
               "Please enter your phone number:";
    writeMsg(info,sockfd);
    client newClient;
    newClient.clientMutex=(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(newClient.clientMutex,NULL);


    while(1)
    {
        fflush(stdout);
        char *msg_ptr;

        numBytesRead = read(sockfd, msgBuffer, BUFFER_SIZE - 1);
        msgBuffer[numBytesRead] = '\0';

        //remove char at the end
        strtok(msgBuffer,"\n");
        //read phone number at first message
        if (firstCon)
        {
            pthread_mutex_lock(newClient.clientMutex);
            //save client info
            strcpy(newClient.phone_number,msgBuffer);
            newClient.socketFd=sockfd;
            printf("%s started session...\n",newClient.phone_number);
            writeMsg("Wellcome\n",sockfd);
            firstCon=false;
            pthread_mutex_unlock(newClient.clientMutex);
        }
        else
        {
            //get command
            msg_ptr = strtok(msgBuffer, " ");

            if(strcmp(msg_ptr,"-whoami")==0)
            {
                writeMsg(newClient.phone_number,sockfd);
                writeMsg("\n",sockfd);
            }
            else if (strcmp(msg_ptr,"-exit")==0)
            {
                //check if connection or group exit
                if((msg_ptr=strtok(NULL," "))==NULL)
                {
                    printf("%s has closed the connection\n",newClient.phone_number);
                    close(sockfd);
                    return NULL;
                }
                else
                {
                    pthread_mutex_lock(globmutex);
                    //search group name from groups
                    for(int i=0; i<MAX_GROUP_NUMBER; i++)
                        if(strcmp(groups[i].name,msg_ptr)==0)
                            //search client in group
                            for(int j=0; j<MAX_CLIENT_PER_GROUP; j++)
                                if(strcmp(groups[i].clients[j].phone_number,newClient.phone_number)==0)
                                {
                                    //remove client from array
                                    for(int k = j; k < MAX_CLIENT_PER_GROUP-1; k++)
                                        groups[i].clients[k] = groups[i].clients[k+1];
                                    groups[i].clientNo--;
                                    writeMsg("Client removed from group\n",sockfd);
                                }
                    pthread_mutex_unlock(globmutex);
                }
            }
            else if(strcmp(msg_ptr,"-gcreate")==0)
            {
                int passBytesRead;
                char passBuffer[BUFFER_SIZE];
                group newGroup;
                newGroup.groupMutex=(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
                pthread_mutex_init(newGroup.groupMutex,NULL);
                pthread_mutex_lock(newGroup.groupMutex);
                pthread_mutex_lock(globmutex);


                msg_ptr=strtok(NULL," ");//get group name
                strcpy(newGroup.name,msg_ptr);
                //get password
                writeMsg("Password:",sockfd);
                passBytesRead = read(sockfd, passBuffer, BUFFER_SIZE - 1);
                passBuffer[passBytesRead] = '\0';
                strtok(passBuffer,"\n");

                strcpy(newGroup.pass,passBuffer);
                writeMsg("Group created successfully\n",sockfd);

                groups[groupNo]=newGroup;
                groupNo++;
                pthread_mutex_unlock(newGroup.groupMutex);
                pthread_mutex_unlock(globmutex);
            }
            else if(strcmp(msg_ptr,"-join")==0)
            {
                int passBytesRead;
                char passBuffer[BUFFER_SIZE];
                char gname[BUFFER_SIZE];
                pthread_mutex_lock(globmutex);
                strcpy(gname,strtok(NULL," "));//get group name
                for(int i=0; i<groupNo; i++)
                {
                    //find group
                    if (strcmp(groups[i].name,gname)==0)
                    {
                        //check password
                        writeMsg("Enter Password:",sockfd);
                        passBytesRead = read(sockfd, passBuffer, BUFFER_SIZE - 1);
                        passBuffer[passBytesRead] = '\0';
                        strtok(passBuffer,"\n");
                        //add client to group
                        if(strcmp(groups[i].pass,passBuffer)==0)
                        {
                            int no=groups[i].clientNo;
                            groups[i].clients[no]=newClient;
                            groups[i].clientNo++;
                            strcpy(newClient.group_name,groups[i].name);
                            writeMsg("User added \n",sockfd);
                        }
                        else
                        {
                            writeMsg("Password Error\n",sockfd);
                        }
                    }
                }
                pthread_mutex_unlock(globmutex);
            }
            else if(strcmp(msg_ptr,"-send")==0)
            {
                pthread_mutex_lock(globmutex);
                char *body=strtok(NULL," ");
                char *msg=txt2JSON(body,newClient.phone_number,newClient.group_name);
                //find clients in group to send txt2json
                for(int i=0; i<MAX_GROUP_NUMBER; i++)
                    if(strcmp(groups[i].name,newClient.group_name)==0)
                        for(int j=0; j<groups[i].clientNo; j++)
                        {
                            writeMsg(msg,groups[i].clients[j].socketFd);
                            writeMsg("\n",groups[i].clients[j].socketFd);
                        }
                pthread_mutex_unlock(globmutex);
            }
        }
        fflush(stdout);
        writeMsg(">",sockfd);
    }

    free(arg); //Free the socket pointer
    //  pthread_exit(pthread_self());

}

int main(void)
{
    //declare vaiables
    struct sockaddr_in server_address,client_address;
    int server_sockfd,client_sockfd;
    socklen_t slen= sizeof(client_address);

    globmutex=(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(globmutex,NULL);

    //Create TCP socket
    if ((server_sockfd=socket(AF_INET,SOCK_STREAM,0)) < 0)
        die("Cannot create socket");

    //initialise IPv4 address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind address to socket
    if(bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))<0)
        die("Socket binding error");

    //listen on socket
    if (listen(server_sockfd,MAXPENDING) < 0)
        die("listen failed");

    // signal(SIGCHLD, SIG_IGN);

    printf("-=- Welcome to Messaging Server -=-\n");
    printf("Server is listening port: %d\n",PORT);
    printf("***********************************\n");

    while(1)
    {
        //fflush(stdout);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &slen);
        if (client_sockfd>0)
        {
            printf("New connection accepted with socket: %d\n",client_sockfd);

            pthread_t client_thread;

            if(pthread_create(&client_thread, NULL,(void *)&connection_handler, (void*)client_sockfd) < 0)
            {
                close(client_sockfd);
                die("Thread cannot be created");
            }

            printf("Handler assigned.\n");
            // pthread_join(client_thread,NULL);
        }
    }
    pthread_mutex_destroy(globmutex);
    free(globmutex);

    if(close(server_sockfd)<0)
        die("Socket closing failed");

    return EXIT_SUCCESS;
}

