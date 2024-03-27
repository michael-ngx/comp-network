#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#define INVALID_SOCKET - 1
const char *LOGIN_CMD = "/login";
const char *LOGOUT_CMD = "/logout";
const char *JOINSESSION_CMD = "/joinsession";
const char *LEAVESESSION_CMD = "/leavesession";
const char *CREATESESSION_CMD = "/createsession";
const char *LIST_CMD = "/list";
const char *QUIT_CMD = "/quit";

void *receive(void *socketfd_void_p);
void *get_in_addr(struct sockaddr *sa);
void login(char *pch, int *socketfd_p, pthread_t *receive_thread_p);
void logout(int *socketfd_p, pthread_t *receive_thread_p);
void joinsession(char *pch, int *socketfd_p);
void leavesession(int socketfd);
void createsession(int socketfd);
void list(int socketfd);
void send_text(int socketfd);

#endif