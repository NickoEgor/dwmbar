#define _POSIX_C_SOURCE 200809L
#define __STDC_WANT_LIB_EXT1__ 1
#include <X11/Xlib.h>
#include <ctype.h>
#include <errno.h>
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
#include <wordexp.h>

#define BUF_LEN(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
    char *name;
    char *command;
    unsigned int interval;
} Block;

#include "config.h"

static Display *dpy;
static int screen;
static Window root;

volatile sig_atomic_t isRunning = 1;
static int isRestart = 0;

#define BAR_MAX_LEN 512
static char oldStatusStr[BAR_MAX_LEN + 1];
static char newStatusStr[BAR_MAX_LEN + 1];

#define CMD_MAX_LEN 512
static char cmd[CMD_MAX_LEN + 1];

#define BLOCKS_AMOUNT (BUF_LEN(blocks))
#define BLOCK_MAX_LEN 128
static char cache[BLOCKS_AMOUNT][BLOCK_MAX_LEN + 1];

#define BLOCKS_PATH_MAX_LEN 450
static char blocks_path[BLOCKS_PATH_MAX_LEN + 1];

#define FIFO_BUFFER_LEN 255

#define EMPTY_OUTPUT "..."
#define LOG_PREFIX "dwmbar: "

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

void resetBuffer(char *buf, size_t size) {
#ifdef __STDC_LIB_EXT1__
    memset_s(buf, size, 0, size);
#else
    memset(buf, 0, size);
#endif
}

void appendBuffer(char *src, const char *dst, size_t size) {
#ifdef __STDC_LIB_EXT1__
    strncat_s(src, size, dst, size);
#else
    strncat(src, dst, size);
#endif
}

void copyBuffer(char *dst, const char *src, size_t size) {
#ifdef __STDC_LIB_EXT1__
    strncpy_s(dst, size, src, size);
#else
    strncpy(dst, src, size);
#endif
}

size_t removeFromBuffer(char *buf, char toRemove) {
    size_t newSize = 0;
    char *read = buf;
    char *write = buf;

    while (*read) {
        /* skip all char for removal */
        while (*read == toRemove) {
            read++;
        }
        /* set next valid char instead of 'toRemove' */
        if (*write != *read) *write = *read;

        read++;
        write++;
        newSize++;
    }
    return newSize;
}

void updateStatus() {
    pthread_mutex_lock(&updateLock);

    /* position where to start writing */
    int k = 0;
    int delimWidth = withSpaces ? 4 : 2;
    /* leave space for last \0 */
    for (size_t i = 0; i < BLOCKS_AMOUNT && k < BAR_MAX_LEN - 1; ++i) {
        /* check i > 0 to skip first block */
        if (i && k + delimWidth < BAR_MAX_LEN - 1) {
            if (withSpaces) newStatusStr[k++] = ' ';
            newStatusStr[k++] = delimiter;
            newStatusStr[k++] = '\n';
            if (withSpaces) newStatusStr[k++] = ' ';
        }

        appendBuffer(newStatusStr + k, cache[i], BAR_MAX_LEN - k - 1);
        k = strlen(newStatusStr);
    }

    /* if new status equals to old then do not update*/
    if (!strcmp(newStatusStr, oldStatusStr)) {
        /* clear new status */
        resetBuffer(newStatusStr, BAR_MAX_LEN);
        pthread_mutex_unlock(&updateLock);
        return;
    }

    /* clear and reset old status */
    resetBuffer(oldStatusStr, BAR_MAX_LEN);
    copyBuffer(oldStatusStr, newStatusStr, BAR_MAX_LEN);

    /* update status */
    setRoot(newStatusStr);

    /* clear new status */
    resetBuffer(newStatusStr, BAR_MAX_LEN);

    pthread_mutex_unlock(&updateLock);
}

/* Opens process *cmd and stores output in *output */
void getCommand(const Block *block, char *output, int button) {
    pthread_mutex_lock(&getLock);

    resetBuffer(output, BLOCK_MAX_LEN);
    copyBuffer(output, EMPTY_OUTPUT, sizeof(EMPTY_OUTPUT));

    FILE *cmdf;
    if (button) {
        sprintf(cmd, "BLOCK_BUTTON=%d %s/%s", button, blocks_path, block->command);
        button = 0;
    } else {
        sprintf(cmd, "%s/%s", blocks_path, block->command);
    }
    cmdf = popen(cmd, "r");
    if (!cmdf) return;

    fgets(output, BLOCK_MAX_LEN, cmdf);
    int newSize = removeFromBuffer(output, '\n');
    output[newSize] = '\0';

    pclose(cmdf);

    pthread_mutex_unlock(&getLock);
}

size_t getBlockIndex(const char *name) {
    for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
        if (strcmp(blocks[i].name, name) == 0) return i;
    }

    return -1;
}

void *periodicUpdater(void *vargp) {
    /* suppress warn */
    (void)vargp;

    const Block *current;
    unsigned int time = 0;

    while (isRunning) {
        for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
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
    // TODO: probably need to get rid of fprintf
    // https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler
    fprintf(stderr, LOG_PREFIX "Signal %d: %s. Stopping...\n", signum, strsignal(signum));
    isRunning = 0;
}

void restartHandler(int signum) {
    isRestart = 1;
    signalHandler(signum);
}

int isNumber(const char *str, unsigned long buf_max_len) {
    unsigned long len = strnlen(str, buf_max_len);
    for (size_t i = 0; i < len; ++i)
        if (!isdigit(str[i])) return 0;
    return 1;
}

void expandBlockCommands() {
    wordexp_t exp_result;
    wordexp(BLOCKS, &exp_result, 0);
    copyBuffer(blocks_path, exp_result.we_wordv[0], BLOCKS_PATH_MAX_LEN);
}

void removeFifoIfExists() {
    if (unlink(fifoPath) == 0) {
        fprintf(stderr, LOG_PREFIX "Removed old file: %s\n", fifoPath);
    } else if (errno != ENOENT) {
        perror(LOG_PREFIX "unlink() failed");
        return;
    }
}

void createFifo() {
    /* create fifo */
    if (mkfifo(fifoPath, 0622) < 0) {
        perror(LOG_PREFIX "mkfifo() failed");
        return;
    }
}

int openFifo() {
    // https://stackoverflow.com/questions/21468856/check-if-file-is-a-named-pipe-fifo-in-c#comment32401662_21468960

    int fifoFd = -1;

    /* open fifo in non-block mode to check type */
    if ((fifoFd = open(fifoPath, O_RDWR | O_NONBLOCK)) < 0) {
        perror(LOG_PREFIX "open() failed");
        return -1;
    }

    /* read fifo stats */
    struct stat fifoStat;
    if (fstat(fifoFd, &fifoStat) < 0) {
        perror(LOG_PREFIX "fstat() failed");
        return -1;
    }

    /* check that file is fifo */
    if (!S_ISFIFO(fifoStat.st_mode)) {
        fprintf(stderr, LOG_PREFIX "File is not FIFO: %s\n", fifoPath);
        return -1;
    }

    return fifoFd;
}

int main(int argc, char **argv) {
    (void)argc;

    fprintf(stderr, LOG_PREFIX "Starting bar...\n");

    /* set periodical updates */
    if (pthread_create(&updaterThreadID, NULL, &periodicUpdater, NULL)) {
        perror(LOG_PREFIX "pthread_create() failed");
        return 1;
    }

    /* install signal handler for SIGINT, SIGHUP */
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    /* install SIGUSR1 for restarting */
    struct sigaction rsa;
    rsa.sa_handler = restartHandler;
    rsa.sa_flags = 0;
    sigemptyset(&rsa.sa_mask);
    sigaction(SIGUSR1, &rsa, NULL);

    /* block signals */
    sigset_t sigset, oldset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, &oldset);

    expandBlockCommands();

    char fifoBuffer[FIFO_BUFFER_LEN + 1] = {0};
    char blockNameBuffer[FIFO_BUFFER_LEN + 1] = {0};

    fd_set readFds;
    fd_set errorFds;

    removeFifoIfExists();
    createFifo();

    while (isRunning) {
        fprintf(stderr, LOG_PREFIX "Opening fifo: %s\n", fifoPath);
        int fifoFd = -1;
        if ((fifoFd = openFifo()) < 0) {
            fprintf(stderr, LOG_PREFIX "Failed to create fifo: %s", fifoPath);
            return 1;
        }

        fprintf(stderr, LOG_PREFIX "Start reading fifo\n");
        while (isRunning) {
            unsigned int button = 0;
            resetBuffer(fifoBuffer, FIFO_BUFFER_LEN);
            resetBuffer(blockNameBuffer, FIFO_BUFFER_LEN);

            FD_ZERO(&readFds);
            FD_ZERO(&errorFds);
            FD_SET(fifoFd, &readFds);

            if (pselect(fifoFd + 1, &readFds, NULL, &errorFds, NULL, &oldset) < 0) {
                perror(LOG_PREFIX "pselect() caught signal");
                isRunning = 0;
                break;
            }

            if (FD_ISSET(fifoFd, &errorFds)) {
                fprintf(stderr, LOG_PREFIX "fifo fd error\n");
                continue;
            }

            if (!FD_ISSET(fifoFd, &readFds)) {
                fprintf(stderr, LOG_PREFIX "fifo fd not in read fdset\n");
                continue;
            }

            int nread = read(fifoFd, fifoBuffer, FIFO_BUFFER_LEN);
            if (nread < 0) {
                perror(LOG_PREFIX "read() failed");
                continue;
            } else if (nread == 0) {
                if (errno == EAGAIN) {
                    fprintf(stderr, LOG_PREFIX "read() EAGAIN\n");
                    continue;
                } else {
                    fprintf(stderr, LOG_PREFIX "read() received EOF\n");
                    // perror(LOG_PREFIX "read() received EOF");
                    break;
                }
            }

            if (sscanf(fifoBuffer, "%s %u\n", blockNameBuffer, &button) < 0) {
                perror(LOG_PREFIX "sscanf() failed");
                continue;
            }

            int block = 0;
            if (isNumber(blockNameBuffer, FIFO_BUFFER_LEN)) {
                /* used for direct clicks on bar */
                block = atoi(blockNameBuffer);
                /* input block starts with 1, not 0, so we decrease it */
                block--;
            } else {
                /* used for updates coming from pipe */
                block = getBlockIndex(blockNameBuffer);
                if (block == -1) {
                    fprintf(stderr, LOG_PREFIX "Invalid block name: %s\n", blockNameBuffer);
                    continue;
                }
            }

            getCommand(&blocks[block], cache[block], button);
            updateStatus();
        }

        fprintf(stderr, LOG_PREFIX "Closing fifo...\n");
        if (close(fifoFd) < 0) {
            perror(LOG_PREFIX "close() failed");
            break;
        }
    }

    fprintf(stderr, LOG_PREFIX "Cleaning up...\n");
    pthread_mutex_destroy(&getLock);
    pthread_mutex_destroy(&updateLock);
    pthread_join(updaterThreadID, NULL);
    removeFifoIfExists();

    if (isRestart) {
        // TODO: https://stackoverflow.com/questions/60691017/how-to-repeatedly-restart-a-program-via-signal

        sigset_t sigs;
        sigprocmask(SIG_SETMASK, &sigs, 0);
        execvp(argv[0], argv);
    }

    return 0;
}
