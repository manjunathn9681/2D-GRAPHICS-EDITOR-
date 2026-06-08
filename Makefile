CC = gcc

all:
	$(CC) graphics.c -o editor

run:
	./editor

clean:
	rm -f editor