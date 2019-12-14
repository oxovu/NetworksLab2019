#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include "../structs.h"

#define CHECK_POLL(i, msg, err_msg) printf(msg);\
                        if (poll(&polls->pollfds[i], 1, -1) < 0) {\
                            perror(err_msg);\
                            return;\
                        }

Client *root;
int sockfd;
Polls *polls;

void add_new_client_to_polls(Client *client) {
    polls->size++;
    polls->pollfds = realloc(polls->pollfds, polls->size * sizeof(Client));
    polls->pollfds[polls->size - 1].fd = client->sockfd;
    polls->pollfds[polls->size - 1].events = POLL_IN;
    client->id = polls->size - 1;
}


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
    if (client == NULL) {
        printf("remove NULL client\n");
        return;
    }

    for (int i = client->id; i < polls->size - 1; ++i) {
        polls->pollfds[i] = polls->pollfds[i + 1];
    }
    polls->size--;
    polls->pollfds = realloc(polls->pollfds, polls->size * sizeof(Client));

    if (client->name != NULL) {
        printf("remove %s ", client->name);
        free(client->name);
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
    free(client);
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
        printf("Removing while get msg.size\n");
        remove_client(client);
        return NULL;
    }

    if (message->size == 0) {
        printf("Removing client with msg.size = 0\n");
        remove_client(client);
        return NULL;
    }
    message->buffer = calloc(message->size, sizeof(char));

    if (read(client->sockfd, message->buffer, message->size) <= 0) {
        printf("Removing while get msg.buffer(msg.size = %d)\n", message->size);
        remove_client(client);
        return NULL;
    }
    return message;
}

void handle_client(Client *client) {
    printf("Processed client = %s, revent = %d\n", client->name,
           polls->pollfds[client->id].revents);


    Message *msg_name = get_new_mess(client->name);

    CHECK_POLL(client->id, "Checking poll in msg_text.size processing...\n", "ERROR on msg_text.size poll")
    CHECK_POLL(client->id, "Checking poll in msg_text.buffer processing...\n", "ERROR on msg_text.buffer poll")
    Message *msg_text = read_mess(client);


    if (msg_text != NULL) {
        printf("%s >> %s\n", msg_name->buffer, msg_text->buffer);
        Client *n_client = root->next;
        while (n_client != NULL) {
            if (n_client != client && n_client->connection) {
                write_mess(n_client, msg_name);
                write_mess(n_client, msg_text);
            }
            n_client = n_client->next;
        }
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
    printf("\nserver exit\n");
    exit(0);
}

int accept_new_client(struct sockaddr_in *cli_addr, unsigned int *clilen) {
    int newsockfd = accept(sockfd, (struct sockaddr *) cli_addr, clilen);

    if (newsockfd <= 0) {
        return -1;
    }

    Client *free_client = root;
    while (free_client->next != NULL) {
        free_client = free_client->next;
    }
    Client *new_client = get_new_client();
    free_client->next = new_client;
    new_client->prev = free_client;

    new_client->sockfd = newsockfd;
    add_new_client_to_polls(new_client);

    printf("Getting name\n");
    Message *name_mess = read_mess(new_client);
    if (name_mess != NULL) {
        new_client->name = name_mess->buffer;
        new_client->connection = true;
        Message welcome_message = {"Welcome", 7};
        write_mess(new_client, &welcome_message);
        printf("New client: name = %s\n", new_client->name);
    }

    return 0;
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

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("ERROR making socket nonblock");
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

    polls = calloc(1, sizeof(Polls));
    polls->size = 1;
    polls->pollfds = calloc(polls->size, sizeof(struct pollfd));
    polls->pollfds[0].fd = sockfd;
    polls->pollfds[0].events = POLLIN;


    for (;;) {
        printf("Checking main poll...\n");
        if (poll(polls->pollfds, polls->size, -1) < 0) {
            perror("ERROR on first poll");
            break;
        }
        printf("Something happened...\n");

        int curr_size = polls->size;
        Client *processed_client = root->next;
        for (int i = 0; i < curr_size; i++) {
            if (i == 0) {//accepting new clients mode
                printf("Accepting new clients\n");
                for (;;) {
                    if (accept_new_client(&cli_addr, &clilen) < 0) {
                        break;
                    }
                }
                printf("All new clients accepted\n");
            } else {//processing clients mode
                if (polls->pollfds[i].revents == 0) {//nothing to do
                    processed_client = processed_client->next;
                    continue;

                } else if (polls->pollfds[i].revents == POLLIN) {//read message from clients
                    handle_client(processed_client);

                } else if (polls->pollfds[i].revents == POLL_ERR) {//bad cases
                    printf("Bad client = %s, revent = POLL_ERR\n", processed_client->name);
                    remove_client(processed_client);

                } else if (polls->pollfds[i].revents == POLL_HUP) {//bad cases
                    printf("Bad client = %s, revent = POLL_HUP\n", processed_client->name);
                    remove_client(processed_client);

                } else {//bad cases
                    printf("Unterminated case\n");
                    server_exit(SIGINT);
                }
                processed_client = processed_client->next;
            }
        }
    }
    return 0;
}


