#pragma once

typedef struct uvwasi_ciovec_s {
  const void* buf;
  size_t buf_len;
} ciovec_t;

__attribute__((
            import_module("wasi_snapshot_preview1"),
            import_name("fd_write")))
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
