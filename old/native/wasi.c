#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <wasi.h>

uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
  if (nwritten == NULL) {
    return EINVAL;
  }

  *nwritten = 0;

  if (iovs == NULL || iovs_len == 0) {
    return 0; // Success, nothing written
  }

  for (size_t i = 0; i < iovs_len; i++) {
    if (iovs[i].buf == NULL && iovs[i].buf_len > 0) {
      return EINVAL;
    }

    if (iovs[i].buf_len == 0) {
      continue;
    }

    ssize_t written = write(fd, iovs[i].buf, iovs[i].buf_len);
    if (written < 0) {
      return errno;
    }

    *nwritten += written;

    // If we wrote less than requested, return early
    if ((size_t)written < iovs[i].buf_len) {
      return 0;
    }
  }

  return 0; // Success
}
