#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../structs.h"
#include <string.h>


void write_mess(int sockfd, Message *message) {
    if (write(sockfd, &message->size, sizeof(int)) <= 0) {
        perror("ERROR writing size");
        exit(1);
    }
    /* Write a response to the client */
    if (write(sockfd, message->buffer, message->size) <= 0) {
        perror("ERROR writing msg");
        exit(1);
    }
}

Message *cmd_mess() {
    Message *message;
    message = calloc(1, sizeof(Message));
    message->buffer = malloc(MAX_MESS_SIZE * sizeof(char));
    bzero(message->buffer, MAX_MESS_SIZE);
    fgets(message->buffer, MAX_MESS_SIZE, stdin);
    char *pos = strrchr(message->buffer, '\n');
    if (pos)
        message->buffer[pos-message->buffer] = 0;
    message->size = (int) strlen(message->buffer);
    return message;
}

void write_loop(int sockfd) {
    while (1) {
        printf("You >> ");
        Message *message = cmd_mess();
        write_mess(sockfd, message);
        free(message->buffer);
        free(message);
    }
}

Message *read_mess(int sockfd) {
    Message *message = calloc(1, sizeof(Message));
    message->size = 0;
    if (read(sockfd, &message->size, sizeof(int)) <= 0) {
        perror("ERROR reading size");
        exit(1);
    }
    message->buffer = calloc(message->size, sizeof(char));
    if (read(sockfd, message->buffer, MAX_MESS_SIZE) <= 0) {
        perror("ERROR reading msg");
        exit(1);
    }
    return message;
}

void read_loop(sockfd) {
    while (1) {
        Message *message = read_mess(sockfd);
        printf("\r%s\nYou >> ", message->buffer);
        fflush(stdout);
        free(message->buffer);
        free(message);
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

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    printf("Enter your name\n");
    write_mess(sockfd,cmd_mess());

    Message *message = read_mess(sockfd);
    printf("%s\n", message->buffer);
    free(message);

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