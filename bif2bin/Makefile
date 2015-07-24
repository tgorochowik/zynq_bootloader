CC=gcc
FILES=bif2bin.c bif_parser.c bootrom.c
CFLAGS=-lpcre -lelf
OUTPUT=bif2bin

all:
	${CC} ${FILES} ${CFLAGS} -o ${OUTPUT}
