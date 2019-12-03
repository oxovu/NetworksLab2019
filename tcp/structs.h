#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>

#ifndef NETWORKSLAB2019_STRUCTS_H
#define NETWORKSLAB2019_STRUCTS_H

#define MAX_MESS_SIZE 256

struct Client {
    int sockfd;
    char *name;
    struct Client *prev;
    struct Client *next;
    bool connection;
    int id;
} typedef Client;

struct Message {
    char *buffer;
    int size;
} typedef Message;

char *concat(char *s1, char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char *cur_time() {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char *res = calloc(7, sizeof(char));
    sprintf(res, "[%d:%d]", timeinfo->tm_hour, timeinfo->tm_min);
    return res;
}

struct Polls {
    struct pollfd *pollfds;
    int size;
} typedef Polls;

#endif
