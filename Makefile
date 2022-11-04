PREFIX = ~/.local
BLOCKS = ~/.local/share/dwmbar/blocks
SOURCES = dwmbar.c
BINARY = dwmbar

.PHONY: build install install-blocks uninstall clean
default: build install

build: config.h
	${CC} -Wall -Wextra -pedantic -o ${BINARY} ${SOURCES} -lpthread -lX11

config.h:
	cp config.def.h $@

install: build
	mkdir -p ${PREFIX}/bin
	cp -f {${BINARY},dwmlistener.sh} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/{${BINARY},dwmlistener.sh}

restart:
	pkill -USR1 $(BINARY)

install-blocks:
	mkdir -p ${BLOCKS}
	cp -rn blocks/* ${BLOCKS}

uninstall:
	rm ${PREFIX}/bin/{dwmbar,dwmlistener.sh}

clean:
	rm -f ${BINARY}
