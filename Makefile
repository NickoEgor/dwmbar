PREFIX = ~/.local
BLOCKS = $(PREFIX)/share/dwmbar/blocks
SOURCES = dwmbar.c
BINARY = dwmbar

.PHONY: config build install run restart install-blocks uninstall clean
default: build install restart

config:
	[ -f config.h ] || cp config.def.h config.h

build: config
	$(CC) -Wall -Wextra -pedantic -o $(BINARY) $(SOURCES) -lpthread -lX11

install:
	[ -f "$(BINARY)" ] || exit 1
	mkdir -p $(PREFIX)/bin
	cp -f {$(BINARY),dwmlistener.sh} $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/{$(BINARY),dwmlistener.sh}

run:
	./$(BINARY)

restart:
	pkill -USR1 $(BINARY)

kill:
	pkill $(BINARY)

install-blocks:
	mkdir -p $(BLOCKS)
	cp -rn blocks/* $(BLOCKS)

uninstall:
	rm $(PREFIX)/bin/{$(BINARY),dwmlistener.sh}

clean:
	rm -f $(BINARY)
