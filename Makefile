PREFIX = ~/.local
CONFIG_PATH = ~/.config/dwmbar
SOURCES = dwmbar.c
BINARY = dwmbar

build:
	cp -n config.def.h config.h
	${CC} -Wall -Wextra -pedantic -o ${BINARY} ${SOURCES} -lpthread -lX11

install: build
	mkdir -p ${CONFIG_PATH}
	cp setup ${CONFIG_PATH}
	cp -r blocks/ ${CONFIG_PATH}
	mkdir -p ${PREFIX}/bin
	cp -f {${BINARY},dwmlistener.sh} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/{${BINARY},dwmlistener.sh}

uninstall:
	rm -rf ${CONFIG_PATH}
	rm ${PREFIX}/bin/{dwmbar,dwmlistener.sh}

clean:
	rm ${BINARY}

.PHONY: build install uninstall clean
