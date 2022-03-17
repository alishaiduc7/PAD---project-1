#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

int cli_cnt = 0;
static int uid = 10;

// Client structure 
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

/* creare coada de clienti */
void queue_add_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }
 
    pthread_mutex_unlock(&clients_mutex);
}

// stergem clientul in momentul in care iese din chat
void queue_remove_client(int uid)
{
    
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

//Trimite mesaj tuturor clientilor cu exceptia celui care a trimis mesajul
void send_message(char *s, int uid)
{

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("Write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Comunicatia cu clientul
void *handle_client(void *arg)
{
    char buff_out[BUFFER_SIZE];
    char name[32];
    int exit_flag = 0;

    cli_cnt++;
    client_t *cli = (client_t *)arg;

    // verifica daca am scris un nume pentru client
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
    {
        printf("Didn't enter the name.\n");
        exit_flag = 1;
    }
    else
    {
        // anunta intrarea in chat a noului client
        strcpy(cli->name, name);
        sprintf(buff_out, "%s has joined\n", cli->name);
        printf("%s", buff_out);
        // afiseaza in terminalul fiecaruia noul client fara terminalul propriu
        send_message(buff_out, cli->uid);
    }
    // reseteaza buffer-ul
    bzero(buff_out, BUFFER_SIZE);

    while (1)
    {
        if (exit_flag)
             break;
        // recv returneaza numarul de bytes de mesaj din buff_out pe care i-a primit din socket
        // daca e -1 returneaza eroare
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0)
        {
            if (strlen(buff_out) > 0)
            {
                send_message(buff_out, cli->uid);

                string_trim(buff_out, strlen(buff_out));
                printf("%s\n", buff_out);
            }
            
        }  // daca socket-ul s-a terminat brusc sau se cere exit 
        else if (receive == 0 || strcmp(buff_out, "exit") == 0)
        {
            sprintf(buff_out, "%s has left\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            exit_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            exit_flag = 1;
        }

        bzero(buff_out, BUFFER_SIZE);
    }

   
    // ca sa nu ramana socket ul deschis dupa ce
    // stergem clientul din coada cand toate de mai sus nu mai sunt valabile
    close(cli->sockfd);
    queue_remove_client(cli->uid);
    free(cli);
    cli_cnt--;
	//obtinem tid de la thread-ul actual si il eliberam
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connection_fd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Setarile socket-ului 
    // AF_INET - refers to the address-family ipv4
    // SOCK_STREAM - connection-oriented TCP protocol
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);
    // htons The htons function returns the value in TCP/IP network byte order.

	//optiunile socket-ului
    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("setsockopt failed");
        return EXIT_FAILURE;
    }

    //bind
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Socket binding failed");
        return EXIT_FAILURE;
	}

    // listen accepta request-urile de conexiune primite de la clienti
    if (listen(listenfd, 10) < 0)
    {
        perror("Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("***** WELCOME *****\n");
    
    while (1)
    {
        socklen_t cli_length = sizeof(cli_addr);
        connection_fd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_length);
       
        // Setarile clientului
        // alocam memorie pt un nou client
        client_t *client = (client_t *)malloc(sizeof(client_t));
        client->address = cli_addr;
        client->sockfd = connection_fd;
        client->uid = uid++;

		//Adaugam clientul in coada si cream thread-ul
        queue_add_client(client);
        pthread_create(&tid, NULL, &handle_client, (void *)client);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}
