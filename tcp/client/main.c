#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../structs.h"
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

struct pollfd *fdreed;


void write_mess(int sockfd, Message *message) {
    if (message->size != 0) {
        if (write(sockfd, &message->size, sizeof(int)) <= 0) {
            perror("ERROR writing size");
            exit(1);
        }
        /* Write a response to the client */
        if (write(sockfd, message->buffer, message->size) <= 0) {
            perror("ERROR writing msg");
            exit(1);
        }
    } else {
        return;
    }
}

Message *cmd_mess() {
    Message *message;
    message = calloc(1, sizeof(Message));
    while (1) {
        char *time = cur_time();
        printf("%s You >> ", time);
        free(time);

        message->buffer = malloc(MAX_MESS_SIZE * sizeof(char));
        bzero(message->buffer, MAX_MESS_SIZE);
        fgets(message->buffer, MAX_MESS_SIZE, stdin);
        char *pos = strrchr(message->buffer, '\n');
        if (pos)
            message->buffer[pos - message->buffer] = 0;
        message->size = (int) strlen(message->buffer);
        if (message->size != 0) {
            break;
        } else {
            printf("Empty line, try again\n");
            free(message->buffer);
        }
    }
    return message;
}

void write_loop(int sockfd) {
    while (1) {
        char *time = cur_time();
        Message *message = cmd_mess();
        write_mess(sockfd, message);
        free(time);
        free_message(message);
    }
}

Message *read_mess(int sockfd) {
    Message *message = calloc(1, sizeof(Message));
    message->size = 0;

    if (read(sockfd, &message->size, sizeof(int)) <= 0) {
        perror("ERROR reading size");
        exit(1);
    }

    int poll_status = poll(fdreed, 1, 5 * 60 * 1000);
    message->buffer = calloc(message->size, sizeof(char));

    if (poll_status < 0) {
        perror("ERROR ON POLL");
        exit(1);

    } else if (poll_status == 0) {
        printf("TIMEOUT WHILE BUFFER GETTING\n");

    } else if (read(sockfd, message->buffer, message->size) <= 0) {
        perror("ERROR reading message");
        exit(1);

    }
    return message;
}

void read_loop(sockfd) {
    int poll_status = 0;
    bool name_got = false;
    Message *msg_name;
    Message *msg_text;

    for (;;) {
        poll_status = poll(fdreed, 1, 5 * 60 * 1000);
        if (poll_status < 0) {
            perror("ERROR ON POLL");
            exit(1);
        } else if (poll_status == 0) {
            printf("TIMEOUT WHILE SIZE GETTING\n");
        } else if (fdreed->revents == 0) {
            continue;
        } else if (fdreed->revents == POLLIN) {
            if (!name_got) {
                msg_name = read_mess(sockfd);
                name_got = true;

            } else {
                msg_text = read_mess(sockfd);
                char *time = cur_time();
                printf("\r%s %s >> %s\n%s You >> ", time, msg_name->buffer, msg_text->buffer, time);
                fflush(stdout);
                free(time);
                free_message(msg_name);
                free_message(msg_text);
                name_got = false;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    uint16_t portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = (uint16_t) atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("ERROR making socket nonblock");
        exit(1);
    }

    printf("Sockfd = %d initialized\n", sockfd);


    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    fdreed = calloc(1, sizeof(struct pollfd));
    fdreed->fd = sockfd;
    fdreed->events = POLLIN;


    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    /* Now connect to the server */
    while (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Server isn't runing");
            exit(1);
        }
    }

    printf("Enter your name\n");
    write_mess(sockfd, cmd_mess());

    int poll_status = poll(fdreed, 1, 5 * 60 * 1000);
    if (poll_status < 0) {
        perror("ERROR ON POLL");
        exit(1);
    } else if (poll_status == 0) {
        printf("TIMEOUT WHILE SIZE GETTING\n");
    } else if (fdreed->revents == 0) {

    } else if (fdreed->revents == POLLIN) {
        Message *message = read_mess(sockfd);
        printf("%s\n", message->buffer);
        free_message(message);

    };

    /*run read thread*/
    pthread_t read;
    pthread_create(&read, NULL, (void *) read_loop, (void *) sockfd);

    /*run write thread*/
    pthread_t write;
    pthread_create(&write, NULL, (void *) write_loop, (void *) sockfd);

    pthread_join(read, NULL);
    pthread_join(write, NULL);
    return 0;
}