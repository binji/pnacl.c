/* Copyright 2016 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_FILESYSTEM_H_
#define PN_FILESYSTEM_H_

static PNErrno pn_from_errno(int e) {
  switch (e) {
#define PN_ERRNO(name, id, str) case name: return PN_##name;
  PN_FOREACH_ERRNO(PN_ERRNO)
#undef PN_ERRNO
    default:
      PN_FATAL("Unknown errno: %d\n", e);
      return PN_ENOSYS;
  }
}

static PNErrno pn_filesystem_get_host_fd(PNExecutor* executor,
                                         uint32_t fd,
                                         int* host_fd) {
  if (fd >= PN_MAX_FDS) {
    PN_TRACE(IRT, "      fd > %d, errno = EBADF\n", PN_MAX_FDS);
    return PN_EBADF;
  }
  *host_fd = executor->fd_map[fd];
  if (*host_fd == -1) {
    PN_TRACE(IRT, "      fd not open, errno = EBADF\n");
    return PN_EBADF;
  }

  return 0;
}

static void pn_filesystem_write_stat_buf(PNExecutor* executor,
                                         uint32_t stat_p,
                                         struct stat* buf) {
  /*
  * offset size
  * 0  8 dev_t     st_dev;
  * 8  8 ino_t     st_ino;
  * 16 4 mode_t    st_mode;
  * 20 4 nlink_t   st_nlink;
  * 24 4 uid_t     st_uid;
  * 28 4 gid_t     st_gid;
  * 32 8 dev_t     st_rdev;
  * 40 8 off_t     st_size;
  * 48 4 blksize_t st_blksize;
  * 52 4 blkcnt_t  st_blocks;
  * 56 8 time_t    st_atime;
  * 64 8 int64_t   st_atimensec;
  * 72 8 time_t    st_mtime;
  * 80 8 int64_t   st_mtimensec;
  * 88 8 time_t    st_ctime;
  * 96 8 int64_t   st_ctimensec;
  * 104 total
  */
  pn_memory_write_u64(executor->memory, stat_p + 0, buf->st_dev);
  pn_memory_write_u64(executor->memory, stat_p + 8, buf->st_ino);
  pn_memory_write_u32(executor->memory, stat_p + 16, buf->st_mode);
  pn_memory_write_u32(executor->memory, stat_p + 20, buf->st_nlink);
  pn_memory_write_u32(executor->memory, stat_p + 24, buf->st_uid);
  pn_memory_write_u32(executor->memory, stat_p + 28, buf->st_gid);
  pn_memory_write_u64(executor->memory, stat_p + 32, buf->st_rdev);
  pn_memory_write_u64(executor->memory, stat_p + 40, buf->st_size);
  pn_memory_write_u32(executor->memory, stat_p + 48, buf->st_blksize);
  pn_memory_write_u32(executor->memory, stat_p + 52, buf->st_blocks);
  pn_memory_write_u64(executor->memory, stat_p + 56, buf->st_atime);
  pn_memory_write_u64(executor->memory, stat_p + 64, buf->st_atim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 72, buf->st_mtime);
  pn_memory_write_u64(executor->memory, stat_p + 80, buf->st_mtim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 88, buf->st_ctime);
  pn_memory_write_u64(executor->memory, stat_p + 96, buf->st_ctim.tv_nsec);
}

static PNErrno pn_filesystem_fdio_close(PNExecutor* executor, uint32_t fd) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_CLOSE(%u)\n", fd);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  if (g_pn_filesystem_access) {
    result = close(host_fd);
  } else {
    PN_CHECK(host_fd <= 2);
    /* Lie and say we closed the fd */
    result = 0;
  }

  if (result == 0) {
    executor->fd_map[fd] = -1;
    return 0;
  } else {
    return pn_from_errno(errno);
  }
}

static PNErrno pn_filesystem_fdio_read(PNExecutor* executor,
                                       uint32_t fd,
                                       uint32_t buf_p,
                                       uint32_t count,
                                       uint32_t nread_p) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_READ(%u, %u, %u, %u)\n", fd, buf_p, count,
           nread_p);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  pn_memory_check(executor->memory, buf_p, count);
  void* buf_pointer = executor->memory->data + buf_p;

  if (!g_pn_filesystem_access) {
    if (host_fd != 0) {
      PN_TRACE(IRT, "      fd != 0, errno = EINVAL\n");
      return PN_EINVAL;
    }
  }

  ssize_t nread = read(host_fd, buf_pointer, count);
  if (nread < 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  pn_memory_write_u32(executor->memory, nread_p, (int32_t)nread);
  PN_TRACE(IRT, "      nread = %d\n", (int32_t)nread);
  return 0;
}

static PNErrno pn_filesystem_fdio_seek(PNExecutor* executor,
                                       uint32_t fd,
                                       uint32_t offset,
                                       uint32_t whence,
                                       uint32_t new_offset_p) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_SEEK(%u, %u, %u, %u)\n", fd, offset, whence,
           new_offset_p);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  if (g_pn_filesystem_access) {
    off_t new_offset = lseek(host_fd, offset, whence);
    if (new_offset < 0) {
      PN_TRACE(IRT, "      errno = %d\n", errno);
      return pn_from_errno(errno);
    }

    pn_memory_write_u32(executor->memory, new_offset_p, (int32_t)new_offset);
    PN_TRACE(IRT, "      new_offset = %d\n", (int32_t)new_offset);
    return 0;
  } else {
    PN_TRACE(IRT, "      errno = ESPIPE\n");
    return PN_ESPIPE;
  }
}

static PNErrno pn_filesystem_fdio_write(PNExecutor* executor,
                                        uint32_t fd,
                                        uint32_t buf_p,
                                        uint32_t count,
                                        uint32_t nwrote_p) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_WRITE(%u, %u, %u, %u)\n", fd, buf_p, count,
           nwrote_p);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  pn_memory_check(executor->memory, buf_p, count);
  void* buf_pointer = executor->memory->data + buf_p;

  if (!g_pn_filesystem_access) {
    if (host_fd != 1 && host_fd != 2) {
      PN_TRACE(IRT, "      fd != 1 && fd != 2, errno = EBADF\n");
      return PN_EBADF;
    }
  }

  ssize_t nwrote = write(host_fd, buf_pointer, count);
  pn_memory_write_u32(executor->memory, nwrote_p, (int32_t)nwrote);
  PN_TRACE(IRT, "      nwrote = %d\n", (int32_t)nwrote);
  return 0;
}

static PNErrno pn_filesystem_fdio_fstat(PNExecutor* executor,
                                        uint32_t fd,
                                        uint32_t stat_p) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_FSTAT(%u, %u)\n", fd, stat_p);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  struct stat buf;
  result = fstat(host_fd, &buf);
  if (result != 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  pn_filesystem_write_stat_buf(executor, stat_p, &buf);
  return 0;
}

static PNErrno pn_filesystem_fdio_isatty(PNExecutor* executor,
                                         uint32_t fd,
                                         uint32_t result_p) {
  PN_TRACE(IRT, "    NACL_IRT_FDIO_ISATTY(%u, %u)\n", fd, result_p);

  int host_fd;
  int result = pn_filesystem_get_host_fd(executor, fd, &host_fd);
  if (result != 0) {
    return result;
  }

  result = isatty(host_fd);
  pn_memory_write_i32(executor->memory, result_p, result);
  PN_TRACE(IRT, "      result_p = %d, errno = 0\n", result);
  return 0;
}

static PNErrno pn_filesystem_filename_getcwd(PNExecutor* executor,
                                             uint32_t pathname_p,
                                             uint32_t len) {
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_GETCWD(%u, %u)\n", pathname_p, len);

  pn_memory_check(executor->memory, pathname_p, len);
  void* pathname = executor->memory->data + pathname_p;
  void* result = NULL;

  if (g_pn_filesystem_access) {
    result = getcwd(pathname, len);
  } else {
    if (strlen(PN_FAKE_GETCWD) + 1 > len) {
      PN_TRACE(IRT, "      len is too small, errno = ERANGE\n");
      return PN_ERANGE;
    }
    memcpy(pathname, PN_FAKE_GETCWD, strlen(PN_FAKE_GETCWD) + 1);
    result = pathname;
  }

  if (result == NULL) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  PN_TRACE(IRT, "      returning (%.*s)\n", len, (char*)pathname);
  return 0;
}

static PNErrno pn_filesystem_filename_open(PNExecutor* executor,
                                           uint32_t pathname_p,
                                           uint32_t oflag,
                                           uint32_t cmode,
                                           uint32_t newfd_p) {
  pn_memory_check_cstr(executor->memory, pathname_p);
  char* pathname = executor->memory->data + pathname_p;
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_OPEN(%u (%s), %u, %u, %u)\n", pathname_p,
           pathname, oflag, cmode, newfd_p);

  if (!g_pn_filesystem_access) {
    PN_TRACE(IRT, "      errno = ENOENT\n");
    return PN_ENOENT;
  }

  int host_fd = open(pathname, oflag, cmode);
  if (host_fd < 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  /* Find the lowest fd to map to */
  uint32_t fd;
  for (fd = 0; fd < PN_MAX_FDS; ++fd) {
    if (executor->fd_map[fd] == -1) {
      break;
    }
  }

  if (fd == PN_MAX_FDS) {
    PN_TRACE(IRT, "      errno = EMFILE\n");
    return PN_EMFILE;
  }

  executor->fd_map[fd] = host_fd;
  pn_memory_write_u32(executor->memory, newfd_p, fd);
  return 0;
}

static PNErrno pn_filesystem_filename_access(PNExecutor* executor,
                                             uint32_t pathname_p,
                                             uint32_t amode) {
  pn_memory_check_cstr(executor->memory, pathname_p);
  char* pathname = executor->memory->data + pathname_p;
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_ACCESS(%u (%s), %u)\n", pathname_p,
           pathname, amode);

  if (!g_pn_filesystem_access) {
    PN_TRACE(IRT, "      errno = ENOENT\n");
    return PN_ENOENT;
  }

  int result = access(pathname, amode);
  if (result != 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  return 0;
}

static PNErrno pn_filesystem_filename_readlink(PNExecutor* executor,
                                               uint32_t path_p,
                                               uint32_t buf_p,
                                               uint32_t count,
                                               uint32_t nread_p) {
  pn_memory_check_cstr(executor->memory, path_p);
  char* path = executor->memory->data + path_p;
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_READLINK(%u (%s), %u, %u, %u)\n", path_p,
           path, buf_p, count, nread_p);

  pn_memory_check(executor->memory, buf_p, count);
  void* buf = executor->memory->data + buf_p;

  if (!g_pn_filesystem_access) {
    PN_TRACE(IRT, "      errno = ENOENT\n");
    return PN_ENOENT;
  }

  ssize_t nread = readlink(path, buf, count);
  if (nread < 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  pn_memory_write_u32(executor->memory, nread_p, (int32_t)nread);
  PN_TRACE(IRT, "      nread = %d\n", (int32_t)nread);
  return 0;
}

static PNErrno pn_filesystem_filename_stat(PNExecutor* executor,
                                           uint32_t pathname_p,
                                           uint32_t stat_p) {
  pn_memory_check_cstr(executor->memory, pathname_p);
  char* pathname = executor->memory->data + pathname_p;

  PN_TRACE(IRT, "    NACL_IRT_FILENAME_STAT(%u (%s), %u)\n", pathname_p,
           pathname, stat_p);

  struct stat buf;
  int result = stat(pathname, &buf);
  if (result != 0) {
    PN_TRACE(IRT, "      errno = %d\n", errno);
    return pn_from_errno(errno);
  }

  pn_filesystem_write_stat_buf(executor, stat_p, &buf);
  return 0;
}

#endif /* PN_FILESYSTEM_H_ */
