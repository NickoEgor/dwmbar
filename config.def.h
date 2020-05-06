/* setup your blocks here */
#define BLOCKSPATH "~/.config/dwmbar/blocks"
static const Block blocks[] = {
    /* command */           /* interval */
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

/* delimiter for blocks */
static char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static int withSpace = 1;

/* path to dwmbar fifo */
static const char fifo[] = "/tmp/dwmbar.fifo";
