#pragma once

#include <wasi.h>

/**
 * @brief Writes all data from the iovecs to the specified file descriptor.
 *
 * This function repeatedly calls fd_write until all data is written or an error occurs.
 * It updates the iovecs to skip already-written data.
 *
 * @param fd The file descriptor to write to.
 * @param iovs Array of ciovec_t structures containing the data to write.
 * @param iovs_len Number of iovecs in the array.
 * @return 0 on success, or an error code if fd_write fails.
 */
uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len);
