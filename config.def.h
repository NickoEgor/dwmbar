/* blocks path */
#define BLOCKS "~/.local/share/dwmbar/blocks"

/* setup your blocks here */
static const Block blocks[] = {
    /* name */       /* command */  /* interval */
    { "player",      "player",      10 },
    { "torrent",     "torrent",     15 },
    { "volume",      "pulse",       5  },
    { "bright",      "brightness",  10 },
    { "internet",    "internet",    5  },
    { "memory",      "memory",      5  },
    { "cpu",         "cpu_bars",    1  },
    { "battery",     "battery",     10 },
    { "temperature", "temperature", 10 },
    { "keyboard",    "keyboard",    10 },
    { "datetime",    "datetime",    60 },
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int withSpaces = 1;

/* path to dwmbar fifo */
static const char fifo[] = "/tmp/dwmbar.fifo";
