ROOT := ..
BASE := $(ROOT)/base
PLATFORM := $(ROOT)/platform
CC ?= clang

UNAME_S := $(shell uname)

COMMON_CFLAGS := -std=c11 -Wall -Wextra -pedantic -I$(ROOT) -I$(ROOT)/base -I$(ROOT)/platform -I.
COMMON_SRCS := \
	core/core.c \
	core/map.c \
	physics/map_physics.c \
	render/text/renderer.c \
	app/text.c \
	$(BASE)/base_io.c \
	$(BASE)/buddy.c \
	$(BASE)/arena.c \
	$(BASE)/scratch.c \
	$(BASE)/format.c \
	$(BASE)/io.c \
	$(BASE)/base_string.c \
	$(BASE)/mem.c \
	$(BASE)/numconv.c \
	$(BASE)/assert.c \
	$(BASE)/exit.c \
	$(BASE)/base_math.c

ifeq ($(UNAME_S),Darwin)
TARGET := fps_text_macos
PLATFORM_SRC := $(PLATFORM)/platform_macos.c
PLATFORM_LDFLAGS := -lSystem -Wl,-e,__start
PLATFORM_CFLAGS := -nostdlib -nostdinc -fno-builtin
else
TARGET := fps_text_linux
PLATFORM_SRC := $(PLATFORM)/platform_linux.c
PLATFORM_LDFLAGS :=
PLATFORM_CFLAGS := -nostdlib -nostdinc -fno-builtin
endif

SRCS := $(COMMON_SRCS) $(PLATFORM_SRC)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean text

all: text

text: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -o $@ $(OBJS) $(PLATFORM_LDFLAGS)

%.o: %.c
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
