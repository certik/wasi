ROOT := ..
BASE := $(ROOT)/base
PLATFORM := $(ROOT)/platform
CC ?= clang

UNAME_S := $(shell uname)

COMMON_CFLAGS := -std=c11 -Wall -Wextra -pedantic -I$(ROOT) -I$(ROOT)/base -I$(ROOT)/platform -I.
CORE_SRCS := \
	core/core.c \
	core/map.c \
	physics/map_physics.c \
	render/text/renderer.c

BASE_SRCS := \
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
PLATFORM_CFLAGS := -nostdlib -fno-builtin
else
TARGET := fps_text_linux
PLATFORM_SRC := $(PLATFORM)/platform_linux.c
PLATFORM_LDFLAGS := -lc
PLATFORM_CFLAGS := -nostdlib -fno-builtin
endif

CORE_OBJS := $(CORE_SRCS:.c=.o)
BASE_OBJS := $(BASE_SRCS:.c=.o)
PLATFORM_OBJ := $(PLATFORM_SRC:.c=.o)

TEXT_APP_SRC := app/text.c
TEXT_APP_OBJ := $(TEXT_APP_SRC:.c=.o)

TEST_CORE_SRC := test/test_core_move.c
TEST_CORE_OBJ := $(TEST_CORE_SRC:.c=.o)
TEST_RENDER_SRC := test/test_renderer_only.c
TEST_RENDER_OBJ := $(TEST_RENDER_SRC:.c=.o)

TEXT_OBJS := $(CORE_OBJS) $(BASE_OBJS) $(PLATFORM_OBJ) $(TEXT_APP_OBJ)
TEST_CORE_OBJS := $(CORE_OBJS) $(BASE_OBJS) $(PLATFORM_OBJ) $(TEST_CORE_OBJ)
TEST_RENDER_OBJS := $(CORE_OBJS) $(BASE_OBJS) $(PLATFORM_OBJ) $(TEST_RENDER_OBJ)

TEST_TARGETS := fps_test_core_move fps_test_renderer_only

.PHONY: all clean text tests

all: text tests

text: $(TARGET)
tests: $(TEST_TARGETS)

$(TARGET): $(TEXT_OBJS)
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -o $@ $(TEXT_OBJS) $(PLATFORM_LDFLAGS)

fps_test_core_move: $(TEST_CORE_OBJS)
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -o $@ $(TEST_CORE_OBJS) $(PLATFORM_LDFLAGS)

fps_test_renderer_only: $(TEST_RENDER_OBJS)
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -o $@ $(TEST_RENDER_OBJS) $(PLATFORM_LDFLAGS)

%.o: %.c
	$(CC) $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) -c $< -o $@

clean:
	rm -f $(CORE_OBJS) $(BASE_OBJS) $(PLATFORM_OBJ) \
		$(TEXT_APP_OBJ) $(TEST_CORE_OBJ) $(TEST_RENDER_OBJ) \
		$(TARGET) $(TEST_TARGETS)
