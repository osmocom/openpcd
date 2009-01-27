CFLAGS=-ggdb -Wall -pedantic -std=gnu99 -O3 -march=nocona

all: openpicc-sniff-14443a openpicc-sniff-both-14443a #decode-14443a_sniff-both

clean: 
	rm openpicc-sniff-{both-,}14443a{,.o} common.o #decode-14443a_sniff-both

openpicc-sniff-14443a: openpicc-sniff-14443a.o common.o
openpicc-sniff-both-14443a: openpicc-sniff-both-14443a.o common.o

.PHONY: test
