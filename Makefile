PREFIX = ~/.local
BLOCKS = ~/.local/share/dwmbar/blocks
SOURCES = dwmbar.c
BINARY = dwmbar

build:
	cp -n config.def.h config.h
	${CC} -Wall -Wextra -pedantic -o ${BINARY} ${SOURCES} -lpthread -lX11

install: build
	mkdir -p ${BLOCKS}
	cp -rn blocks/* ${BLOCKS}
	mkdir -p ${PREFIX}/bin
	cp -f {${BINARY},dwmlistener.sh} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/{${BINARY},dwmlistener.sh}

uninstall:
	rm -rf ${BLOCKS}
	rm ${PREFIX}/bin/{dwmbar,dwmlistener.sh}

clean:
	rm -f ${BINARY}

.PHONY: build install uninstall clean
