
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2

all: libgatorstomp_audio.a

audio_io.o: audio_io.c audio_io.h
	$(CC) $(CFLAGS) -c audio_io.c -o audio_io.o

libgatorstomp_audio.a: audio_io.o
	$(AR) rcs $@ $^

clean:
	rm -f audio_io.o libgatorstomp_audio.a

.PHONY: all clean
