#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

// Variabile globale

int flag = 0;
int sockfd = 0;
char name[32];

// pune endline la \n
void string_trim(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { // trim \n
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

//setam flag-ul pentru deconectarea clientului
void set_flag_on_exit()
{
    flag = 1;
}

void send_msg_handler()
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    while (1)
    {
        fgets(message, LENGTH, stdin);
        
        if(strcmp(message,"\n")!=0){
        
		string_trim(message, LENGTH);

		if (strcmp(message, "exit") == 0)
		{
		    break;
		}
		else
		{
		    // Nume: mesaj
		    sprintf(buffer, "%s: %s\n", name, message);
			//send transmite mesajul altui socket
		    send(sockfd, buffer, strlen(buffer), 0);
		}
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
    set_flag_on_exit();
}

void receive_msg_handler()
{
    char message[LENGTH] = {};
    while (1)
    {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0)
        {
            printf("%s", message);
            
        }
        else if (receive == 0)
        {
            break;
        }
        else
        {
            perror("receive message failed!");
        }
        memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1"; //adresa standard pentru ipv4
    int port = atoi(argv[1]);

   // signal(SIGINT, set_flag_on_exit);

    struct sockaddr_in server_addr;

    /* Socket settings */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        printf("server connection failed\n");
        return EXIT_FAILURE;
    }
    FILE *fis;
    int ok = 0;
    char username[32], password[100], line[100] = {0}, *token, pass[100];
    if ((fis = fopen("databasefile.txt", "a+")) == NULL)
    {
        printf("file open failed\n");
        exit(EXIT_FAILURE);
    }
    printf("***** WELCOME *****\n");
    printf("Introduce your credentials\n");
    
    printf("Enter your username: ");
    scanf("%s", username);
    while (fgets(line, 100, fis))
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        if (strcmp(token, username) == 0)
        {
            ok = 1;
            token = strtok(NULL, " ");
            token = strtok(NULL, " ");
            strcpy(pass, token);
            pass[strlen(pass) - 1] = '\0';
        }
    }
    printf("Enter your password: ");
    scanf("%s", password);
    if (ok == 1)
    {
        if (strcmp(pass, password) != 0)
        {
            printf("Your password is incorrect!\n");
            return -1;
        }
    }
    else
    {
        fprintf(fis, "Username: %s ", username);
        fprintf(fis, "Password: %s", password);
        fprintf(fis, "\n");
    }
    fflush(fis);
    strcpy(name, username);

    // Trimite numele
    send(sockfd, name, 32, 0);
    
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0)
    {
        printf("pthread error\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    // creeaza thread ul prin care primeste celelalte mesaje
    if (pthread_create(&recv_msg_thread, NULL, (void *)receive_msg_handler, NULL) != 0)
    {
        printf("pthread\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);
    fclose(fis);
    return EXIT_SUCCESS;
}
