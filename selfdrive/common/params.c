#include "selfdrive/common/params.h"

#include "selfdrive/common/util.h"

#define _GNU_SOURCE
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int write_db_value(const char* params_path, const char* key, const char* value,
                   size_t value_size) {
  int lock_fd = -1;
  int tmp_fd = -1;
  int result;
  char tmp_path[1024];
  char path[1024];

  // Write value to temp.
  result =
      snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp_value_XXXXXX", params_path);
  if (result < 0) {
    goto cleanup;
  }

  tmp_fd = mkstemp(tmp_path);
  const ssize_t bytes_written = write(tmp_fd, value, value_size);
  if (bytes_written != value_size) {
    result = -20;
    goto cleanup;
  }

  result = snprintf(path, sizeof(path), "%s/.lock", params_path);
  if (result < 0) {
    goto cleanup;
  }
  lock_fd = open(path, 0);

  result = snprintf(path, sizeof(path), "%s/d/%s", params_path, key);
  if (result < 0) {
    goto cleanup;
  }

  // Take lock.
  result = flock(lock_fd, LOCK_EX);
  if (result < 0) {
    goto cleanup;
  }

  // Move temp into place.
  result = rename(tmp_path, path);
  if (result < 0) {
    goto cleanup;
  }

  // fsync to force persist the changes.
  result = fsync(tmp_fd);
cleanup:
  // Release lock.
  if (lock_fd >= 0) {
    close(lock_fd);
  }
  if (tmp_fd >= 0) {
    if (result < 0) {
      remove(tmp_path);
    }
    close(tmp_fd);
  }
  return result;
}

int read_db_value(const char* params_path, const char* key, char** value,
                  size_t* value_sz) {
  int lock_fd = -1;
  int result;
  char path[1024];

  result = snprintf(path, sizeof(path), "%s/.lock", params_path);
  if (result < 0) {
    goto cleanup;
  }
  lock_fd = open(path, 0);

  result = snprintf(path, sizeof(path), "%s/d/%s", params_path, key);
  if (result < 0) {
    goto cleanup;
  }

  // Take lock.
  result = flock(lock_fd, LOCK_EX);
  if (result < 0) {
    goto cleanup;
  }

  // Read value.
  // TODO(mgraczyk): If there is a lot of contention, we can release the lock
  //                 after opening the file, before reading.
  *value = read_file(path, value_sz);
  if (*value == NULL) {
    result = -22;
    goto cleanup;
  }

  // Remove one for null byte.
  if (value_sz != NULL) {
    *value_sz -= 1;
  }
  result = 0;

cleanup:
  // Release lock.
  if (lock_fd >= 0) {
    close(lock_fd);
  }
  return result;
}

void read_db_value_blocking(const char* params_path, const char* key,
                            char** value, size_t* value_sz) {
  while (1) {
    const int result = read_db_value(params_path, key, value, value_sz);
    if (result == 0) {
      return;
    } else {
      // Sleep for 0.1 seconds.
      usleep(100000);
    }
  }
}
