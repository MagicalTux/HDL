#!/bin/make
CC=gcc
CFLAGS=-O2 -Wall -pipe -std=gnu99

all: fpga_prog

fpga_prog: fpga_prog.o
	$(CC) $(CFLAGS) -o $@ $^ -L/opt/lib64 -lftd2xx

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) fpga_prog fpga_prog.o

