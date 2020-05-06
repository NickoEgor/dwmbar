#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>

#define LEN(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
    char* command;
    unsigned int interval;
} Block;

#define BLOCKSPATH "~/.config/dwmbar/blocks"
static const Block blocks[] = {
    { BLOCKSPATH"/player",      10 },
    { BLOCKSPATH"/torrent",     15 },
    { BLOCKSPATH"/volume",      5  },
    { BLOCKSPATH"/brightness",  10 },
    { "xkb-switch",             10 },
    { BLOCKSPATH"/internet",    5  },
    { BLOCKSPATH"/battery",     10 },
    { BLOCKSPATH"/temperature", 10 },
    { BLOCKSPATH"/datetime",    60 }
};

static char delimiter = '|';
static int withSpace = 1;
static const char fifo[] = "/tmp/dwmbar.fifo";

#define BLOCKVARLEN 13
#define CMDMAXLEN 256
#define BLOCKMAXLEN 128
#define BARMAXLEN 512

static Display *dpy;
static int screen;
static Window root;

static int running = 1;

static char oldStatusStr[BARMAXLEN];
static char newStatusStr[BARMAXLEN];
static char blockVar[BLOCKVARLEN] = "BLOCK_BUTTON";
static char cmd[BLOCKVARLEN + CMDMAXLEN];
static char cache[LEN(blocks)][BLOCKMAXLEN] = {0};

static int fifoFd;

pthread_t listenerThreadID;

void setRoot(char *status) {
    Display *d = XOpenDisplay(NULL);
    if (d) {
        dpy = d;
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XStoreName(dpy, root, status);
    XCloseDisplay(dpy);
}

void updateStatus() {
    /* position where to start writing */
    int k = 0;
    /* leave space for last \0 */
    for (size_t i = 0; i < LEN(blocks) && k < BARMAXLEN - 1; ++i) {
        /* skip first block */
        if (i) newStatusStr[k++] = delimiter;

        strncat(newStatusStr + k, cache[i], BARMAXLEN - k - 1);
        k = strlen(newStatusStr);
    }

    /* if new status equals to old then do not update*/
    if (!strcmp(newStatusStr, oldStatusStr)) {
        /* clear new status */
        memset(newStatusStr, 0, strlen(oldStatusStr));
        return;
    }

    /* clear and reset old status */
    memset(oldStatusStr, 0, strlen(oldStatusStr));
    strcpy(oldStatusStr, newStatusStr);

    /* update status */
    setRoot(newStatusStr);

    /* clear new status */
    memset(newStatusStr, 0, strlen(oldStatusStr));
}

void remove_all(char *str, char to_remove) {
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
void getcmd(const Block *block, char *output, int button) {
    FILE *cmdf;
    if (button) {
        sprintf(cmd, "%s=%d %s", blockVar, button, block->command);
        button = 0;
        cmdf = popen(cmd, "r");
    } else {
        strcpy(cmd, block->command);
        cmdf = popen(cmd, "r");
    }
    if (!cmdf)
        return;

    int i = 0;
    if (withSpace)
        output[i++] = ' ';

    fgets(output+i, BLOCKMAXLEN-i, cmdf);
    remove_all(output, '\n');
    i = strlen(output);

    if (withSpace)
        output[i++] = ' ';

    output[i] = '\0';
    pclose(cmdf);
}

void *listenerFifo(void *vargp) {
    (void)vargp; // suppress warn

    char buffer[256];
    int block;
    unsigned int button;

    while (running) {
        block = 0;
        button = 0;
        memset(buffer, 0, strlen(buffer));

        if ((fifoFd = open(fifo, O_RDONLY | O_RSYNC)) < 0) {
            perror("open");
            return NULL;
        }

        if (read(fifoFd, buffer, LEN(buffer) - 1) < 0) {
            perror("read");
            continue;
        }

        if (close(fifoFd) < 0)
            perror("close");

        if (sscanf(buffer, "%d %u\n", &block, &button) < 0) {
            perror("sscanf");
            continue;
        }

        getcmd(&blocks[block-1], cache[block-1], button);
        updateStatus();
    }

    return NULL;
}

void cleanup() {
    pthread_join(listenerThreadID, NULL);
}

void termhandler(int signum) {
    (void)signum; // suppress warn

	running = 0;
    cleanup();
}

/* int main(int argc, char** argv) { */
int main() {
    if (mkfifo(fifo, 0622) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    if (pthread_create(&listenerThreadID, NULL, &listenerFifo, NULL)) {
        perror("pthread_create");
        return 1;
    }

	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	signal(SIGHUP, termhandler);

    const Block* current;
    unsigned int time = 0;

    while (running) {
        for(size_t i = 0; i < LEN(blocks); i++) {
            current = blocks + i;
            if (time % current->interval == 0)
                getcmd(current, cache[i], 0);
        }
        updateStatus();
        sleep(1);
        time++;
    }

    return 0;
}
