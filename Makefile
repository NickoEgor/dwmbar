PREFIX = ~/.local
BLOCKS = ~/.local/share/dwmbar/blocks
SOURCES = dwmbar.c
BINARY = dwmbar

build:
	cp -n config.def.h config.h
	${CC} -DBLOCKS='"${BLOCKS}/"' -Wall -Wextra -pedantic -o ${BINARY} ${SOURCES} -lpthread -lX11

install: build
	mkdir -p ${BLOCKS}
	cp -r blocks/* ${BLOCKS}
	mkdir -p ${PREFIX}/bin
	cp -f {${BINARY},dwmlistener.sh} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/{${BINARY},dwmlistener.sh}

uninstall:
	rm -rf ${BLOCKS_PATH}
	rm ${PREFIX}/bin/{dwmbar,dwmlistener.sh}

clean:
	rm ${BINARY}

.PHONY: build install uninstall clean
