CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic

SOURCES := fps_core.c fps_mock_plugins.c fps_renderer.c fps_assets.c fps_platform_mock.c main.c
OBJECTS := $(SOURCES:.c=.o)
TARGET := fps_demo

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
