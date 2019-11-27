#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>
#include <pthread.h>
#include <tcp/structs.h>

Client *root;
int sockfd;
pthread_mutex_t mutex;

Client *get_new_client() {
    Client *new_client = calloc(1, sizeof(Client));
    new_client->next = NULL;
    return new_client;
};

Message *get_new_mess(char *buffer) {
    Message *message = calloc(1, sizeof(Message));
    message->size = strlen(buffer);
    message->buffer = buffer;
    return message;
}

void remove_client(Client *client) {
    close(client->sockfd);
    if (client->prev != NULL && client->next != NULL) {
        client->prev->next = client->next;
        client->next->prev = client->prev;
    }
    free(client);
    pthread_cancel(client->pthread);
}

void write_mess(Client *client, Message *message) {
    if (write(client->sockfd, &message->size, sizeof(int)) <= 0 ||
        write(client->sockfd, message->buffer, message->size) <= 0) {
        remove_client(client);
    }
}

Message *read_mess(Client *client) {
    Message *message = calloc(1, sizeof(Message));
    message->size = 0;

    if (read(client->sockfd, &message->size, sizeof(int)) <= 0) {
        remove_client(client);
    }

    message->buffer = calloc(message->size, sizeof(char));

    if (read(client->sockfd, message->buffer, message->size) <= 0) {
        remove_client(client);
    }

    return message;
}

void handle_client(Client *client) {
    client->connection = true;

    while (1) {
        Message *message = read_mess(client);
        message->buffer = concat(concat(client->name, ">>"), message->buffer);
        message = get_new_mess(message->buffer);

        printf("%s:%s\n", client->name, message->buffer);

        Client *n_client = root->next;
        while (n_client != NULL) {
            if (n_client != client && n_client->connection == true) {
                write_mess(n_client, message);
            }
            n_client = n_client->next;
        }
    }
}

void server_exit(int sig) {
    Client *client = root;
    if (client != NULL) {
        while (client->next != NULL) {
            client = client->next;
            remove_client(client);
        }
    }
    remove_client(client);
    close(sockfd);
    pthread_mutex_destroy(&mutex);
    printf("server exit\n");
    exit(0);
}


int main(int argc, char *argv[]) {
    int newsockfd;
    uint16_t portno;
    unsigned int clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (signal(SIGINT, server_exit) == SIG_ERR) {
        perror("ERROR server_exit initialization failed");
        exit(1);
    }

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }


    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5001;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Now start listening for the clients, here process will
       * go in sleep mode and will wait for the incoming connection
    */

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    root = get_new_client();
    root->name = "Root";

    while (1) {
        Client *new_client = root;
        int i = 1;
        while (new_client->next != NULL) {
            printf("Client %d = %s", i, new_client->name);
            i++;
            new_client = new_client->next;
        }
        new_client->next = get_new_client();
        new_client->next->prev = new_client;
        new_client = new_client->next;

        /* Accept actual connection from the client */

        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        new_client->sockfd = newsockfd;

        Message *name_mess = read_mess(new_client);
        new_client->name = name_mess->buffer;
        printf("New client.name = %s accepted\n", new_client->name);
        pthread_create(&new_client->pthread, NULL, (void *(*)(void *)) handle_client, new_client);
    }
    return 0;
}


