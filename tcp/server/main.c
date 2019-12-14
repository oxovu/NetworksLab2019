#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include "../structs.h"

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
    message->buffer = strdup(buffer);
    return message;
}

void remove_client(Client *client) {
    if (client == NULL) {
        printf("remove NULL client\n");
        return;
    }
    pthread_mutex_lock(&mutex);
    if (client->name != NULL) {
        printf("remove %s ", client->name);
    } else {
        printf("remove not initialized client\n");
    }
    if (client->prev != NULL) {
        printf("prev = %s ", client->prev->name);
        client->prev->next = client->next;
    }
    if (client->next != NULL) {
        printf("next = %s", client->next->name);
        client->next->prev = client->prev;
    }
    printf("\n");

    close(client->sockfd);
    free(client->name);
    pthread_t cl_thread = client->pthread;
    free(client);
    pthread_mutex_unlock(&mutex);
    pthread_cancel(cl_thread);
}

void write_mess(Client *client, Message *message_name, Message *message_text) {
    if (write(client->sockfd, &message_name->size, sizeof(int)) <= 0 ||
        write(client->sockfd, message_name->buffer, message_name->size) <= 0 ||
        write(client->sockfd, &message_text->size, sizeof(int)) <= 0 ||
        write(client->sockfd, message_text->buffer, message_text->size) <= 0) {
        remove_client(client);
    }
}

Message *read_mess(Client *client) {
    Message *message = calloc(1, sizeof(Message));
    message->size = 0;

    if (read(client->sockfd, &message->size, sizeof(int)) <= 0) {
        printf("Removing while get msg.size\n");
        remove_client(client);
    }

    if (message->size == 0) {
        printf("Removing client with msg.size = 0\n");
        remove_client(client);
    }
    message->buffer = calloc(message->size, sizeof(char));

    if (read(client->sockfd, message->buffer, message->size) <= 0) {
        printf("Removing while get msg.buffer(msg.size = %d)\n", message->size);
        remove_client(client);
    }
    return message;
}

void handle_client(Client *client) {
    Message *msg_name = read_mess(client);
    Message *msg_text;

    client->name = strdup(msg_name->buffer);
    printf("New client %s\n", client->name);
    free_message(msg_name);

    msg_name = get_new_mess("Server");
    msg_text = get_new_mess("Welcome");
    write_mess(client, msg_name, msg_text);
    free_message(msg_name);
    free_message(msg_text);


    client->connection = true;

    while (1) {
        msg_text = read_mess(client);
        msg_name = get_new_mess(client->name);

        printf("%s:%s\n", msg_name->buffer, msg_text->buffer);
        pthread_mutex_lock(&mutex);
        Client *n_client = root->next;
        while (n_client != NULL) {
            if (n_client != client && n_client->connection == true) {
                write_mess(n_client, msg_name, msg_text);
            }
            n_client = n_client->next;
        }
        pthread_mutex_unlock(&mutex);
    }
}

void server_exit(int sig) {
    printf("SIGINT catched\n");
    Client *client = root;
    if (client != NULL) {
        while (client->next != NULL) {
            client = client->next;
            remove_client(client->prev);
        }
    }
//    remove_client(client);            //strange that its not needed

    close(sockfd);
    pthread_mutex_destroy(&mutex);
    printf("\nserver exit\n");
    exit(0);
}

void accept_new_client(struct sockaddr_in *cli_addr, unsigned int *clilen) {
    int newsockfd;
    if ((newsockfd = accept(sockfd, (struct sockaddr *) cli_addr, clilen)) < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    Client *new_client = root;
    int i = 1;
    pthread_mutex_lock(&mutex);
    while (new_client->next != NULL) {
        printf("Client %d = %s\n", i, new_client->next->name);
        i++;
        new_client = new_client->next;
    }

    new_client->next = get_new_client();
    new_client->next->prev = new_client;
    new_client = new_client->next;
    new_client->sockfd = newsockfd;

    pthread_mutex_unlock(&mutex);
    pthread_create(&new_client->pthread, NULL, (void *(*)(void *)) handle_client, new_client);


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
    root->name = strdup("Root");

    while (1) {
        /* Accept actual connection from the client */
        accept_new_client(&cli_addr, &clilen);

    }
    return 0;
}


