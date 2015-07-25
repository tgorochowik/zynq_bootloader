CC=gcc
FILES=src/{bif2bin.c,bif.c,bootrom.c}
CFLAGS=-lpcre -lelf
TARGET=bif2bin

all:
	${CC} ${FILES} ${CFLAGS} -o ${TARGET}

clean:
	rm ${TARGET}
