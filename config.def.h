/* setup your blocks here */
#define PATH "~/.config/dwmbar/blocks"
static const Block blocks[] = {
    /* name */  /* command */           /* interval */
    { "player",      PATH"/player",      10 },
    { "torrent",     PATH"/torrent",     15 },
    { "volume",      PATH"/pulse",       5  },
    { "bright",      PATH"/brightness",  10 },
    { "internet",    PATH"/internet",    5  },
    { "battery",     PATH"/battery",     10 },
    { "temperature", PATH"/temperature", 10 },
    { "keyboard",    PATH"/keyboard",    10 },
    { "datetime",    PATH"/datetime",    60 }
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int withSpaces = 1;

/* path to dwmbar fifo */
static const char fifo[] = "/tmp/dwmbar.fifo";
