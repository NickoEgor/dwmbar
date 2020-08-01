#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LEN(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
    char *name;
    char *command;
    unsigned int interval;
} Block;

#include "config.h"

#define BLOCKVARLEN 13
#define CMDMAXLEN 256
#define BLOCKMAXLEN 128
#define BARMAXLEN 512

static Display *dpy;
static int screen;
static Window root;

static int running = 1;
static int restart = 0;

static char oldStatusStr[BARMAXLEN];
static char newStatusStr[BARMAXLEN];
static char cmd[BLOCKVARLEN + CMDMAXLEN];
static char cache[LEN(blocks)][BLOCKMAXLEN] = {0};
static const char buttonVarName[BLOCKVARLEN] = "BLOCK_BUTTON";

static int fifoFd;

pthread_t updaterThreadID;
pthread_mutex_t updateLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t getLock = PTHREAD_MUTEX_INITIALIZER;

void setRoot(char *status) {
    Display *d = XOpenDisplay(NULL);
    if (d) dpy = d;
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XStoreName(dpy, root, status);
    XCloseDisplay(dpy);
}

void updateStatus() {
    pthread_mutex_lock(&updateLock);

    /* position where to start writing */
    int k = 0;
    int delimWidth = withSpaces ? 4 : 2;
    /* leave space for last \0 */
    for (size_t i = 0; i < LEN(blocks) && k < BARMAXLEN - 1; ++i) {
        /* check i > 0 to skip first block */
        if (i && k + delimWidth < BARMAXLEN - 1) {
            if (withSpaces) newStatusStr[k++] = ' ';
            newStatusStr[k++] = delimiter;
            newStatusStr[k++] = '\n';
            if (withSpaces) newStatusStr[k++] = ' ';
        }

        strncat(newStatusStr + k, cache[i], BARMAXLEN - k - 1);
        k = strlen(newStatusStr);
    }

    /* if new status equals to old then do not update*/
    if (!strcmp(newStatusStr, oldStatusStr)) {
        /* clear new status */
        memset(newStatusStr, 0, strlen(oldStatusStr));
        pthread_mutex_unlock(&updateLock);
        return;
    }

    /* clear and reset old status */
    memset(oldStatusStr, 0, strlen(oldStatusStr));
    strcpy(oldStatusStr, newStatusStr);

    /* update status */
    setRoot(newStatusStr);

    /* clear new status */
    memset(newStatusStr, 0, strlen(oldStatusStr));

    pthread_mutex_unlock(&updateLock);
}

void removeAll(char *str, char to_remove) {
    char *read = str;
    char *write = str;
    while (*read) {
        if (*read == to_remove) {
            read++;
            *write = *read;
        }
        read++;
        write++;
    }
}

/* Opens process *cmd and stores output in *output */
void getCommand(const Block *block, char *output, int button) {
    pthread_mutex_lock(&getLock);

    strcpy(output, "...");

    FILE *cmdf;
    if (button) {
        sprintf(cmd, "%s=%d %s", buttonVarName, button, block->command);
        button = 0;
        cmdf = popen(cmd, "r");
    } else {
        strcpy(cmd, block->command);
        cmdf = popen(cmd, "r");
    }
    if (!cmdf) return;

    fgets(output, BLOCKMAXLEN, cmdf);
    removeAll(output, '\n');
    int i = strlen(output);
    output[i] = '\0';

    pclose(cmdf);

    pthread_mutex_unlock(&getLock);
}

size_t getBlockIndex(const char *name) {
    for (size_t i = 0; i < LEN(blocks); i++) {
        if (strcmp(blocks[i].name, name) == 0)
            return i;
    }

    return -1;
}

void *periodUpdater(void *vargp) {
    (void)vargp;  // suppress warn

    const Block *current;
    unsigned int time = 0;

    while (running) {
        for (size_t i = 0; i < LEN(blocks); i++) {
            current = blocks + i;
            if (time % current->interval == 0) getCommand(current, cache[i], 0);
        }
        updateStatus();
        sleep(1);
        time++;
    }

    return NULL;
}

void signalHandler(int signum) {
    (void)signum;  // suppress warn

    fprintf(stderr, "Stopping...\n");
    running = 0;
}

void restartHandler(int signum) {
    restart = 1;
    signalHandler(signum);
}

int isnumber(const char *str) {
    for (size_t i = 0; i < strlen(str); ++i)
        if (!isdigit(str[i]))
            return 0;
    return 1;
}

int main(int argc, char **argv) {
    (void)argc;
    fprintf(stderr, "Starting bar...\n");

    if (mkfifo(fifo, 0622) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    if (pthread_create(&updaterThreadID, NULL, &periodUpdater, NULL)) {
        perror("pthread_create");
        return 1;
    }

    // Install the signal handler for SIGINT, SIGHUP
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    // Install SIGUSR1 for restarting
    struct sigaction rsa;
    rsa.sa_handler = restartHandler;
    rsa.sa_flags = 0;
    sigemptyset(&rsa.sa_mask);
    sigaction(SIGUSR1, &rsa, NULL);

    // Block signals
    sigset_t sigset, oldset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, &oldset);

    char buffer[256];
    char id[256];
    int block;
    unsigned int button;

    while (running) {
        block = 0;
        button = 0;
        memset(buffer, 0, strlen(buffer));

        if ((fifoFd = open(fifo, O_RDONLY | O_RSYNC | O_NONBLOCK)) < 0) {
            perror("open");
            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fifoFd, &readfds);

        int ready = pselect(fifoFd + 1, &readfds, NULL, NULL, NULL, &oldset);
        if (ready >= 0) {
            if (read(fifoFd, buffer, LEN(buffer) - 1) < 0) {
                perror("read");
                continue;
            }

            if (close(fifoFd) < 0) perror("close");


            if (sscanf(buffer, "%s %u\n", id, &button) < 0) {
                perror("sscanf");
                continue;
            }

            if (isnumber(id)) {
                block = atoi(id);
                /* input block starts with 1, not 0 */
                block--;
            } else {
                block = getBlockIndex(id);
                if (block == -1) {
                    perror("invalid block name");
                    continue;
                }
            }
            getCommand(&blocks[block], cache[block], button);
            updateStatus();
        } else {
            perror("Signal");
        }
    }

    fprintf(stderr, "Cleaning up...\n");
    pthread_mutex_destroy(&getLock);
    pthread_mutex_destroy(&updateLock);
    pthread_join(updaterThreadID, NULL);

    if (restart) {
        sigset_t sigs;
        sigprocmask(SIG_SETMASK, &sigs, 0);
        execvp(argv[0], argv);
    }

    return 0;
}
