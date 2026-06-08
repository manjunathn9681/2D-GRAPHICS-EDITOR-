CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11
LDFLAGS = -lncurses -lm

TARGET  = editor
SRCS    = main.c graphics.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c graphics.h
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
