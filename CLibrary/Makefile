CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
TARGET := build/library
SOURCES := \
	src/main.c \
	src/ui.c \
	src/input.c \
	src/storage.c \
	src/catalogue.c \
	src/circulation.c \
	src/search.c \
	src/seed.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) include/library.h src/storage.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

build:
	mkdir -p build

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rmdir build 2>/dev/null || true
