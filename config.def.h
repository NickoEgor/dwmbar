/* setup your blocks here */
static const Block blocks[] = {
    /* name */       /* command */  /* interval */
    { "player",      BLOCKS"player",      10 },
    { "torrent",     BLOCKS"torrent",     15 },
    { "volume",      BLOCKS"pulse",       5  },
    { "bright",      BLOCKS"brightness",  10 },
    { "internet",    BLOCKS"internet",    5  },
    { "memory",      BLOCKS"memory",      5  },
    { "cpu",         BLOCKS"cpu_bars",    1  },
    { "battery",     BLOCKS"battery",     10 },
    { "temperature", BLOCKS"temperature", 10 },
    { "keyboard",    BLOCKS"keyboard",    10 },
    { "datetime",    BLOCKS"datetime",    60 },
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int withSpaces = 1;

/* path to dwmbar fifo */
static const char fifo[] = "/tmp/dwmbar.fifo";
