src = $(wildcard *.cpp)
obj = $(src:.c=.o)
CC=g++

CFLAGS = -Wall

decoder: $(obj)
		$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean
clean:
		rm -f $(obj) myprog