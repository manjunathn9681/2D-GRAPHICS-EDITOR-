CC     = clang
CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
         -Wcast-align -Wconversion -Wsign-conversion \
         -Wnull-dereference -g3 -O0
OUTDIR = ./build/Debug
TARGET = $(OUTDIR)/outDebug

all: $(TARGET)

$(TARGET): graphics.c graphics.h
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) graphics.c -lm -o $(TARGET)

run: all
	$(TARGET)

clean:
	rm -f $(TARGET)