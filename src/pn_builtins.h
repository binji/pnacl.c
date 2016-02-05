/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_BUILTINS_H_
#define PN_BUILTINS_H_

#define PN_BUILTIN_ARG(name, n, ty)                                  \
  PNRuntimeValue value##n = pn_thread_get_value(thread, arg_ids[n]); \
  pn_##ty name = value##n.ty;                                        \
  (void) name /* no semicolon */

static PNRuntimeValue pn_builtin_NACL_IRT_QUERY(PNThread* thread,
                                                PNFunction* function,
                                                uint32_t num_args,
                                                PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(name_p, 0, u32);
  PN_BUILTIN_ARG(table, 1, u32);
  PN_BUILTIN_ARG(table_size, 2, u32);

  PNMemory* memory = executor->memory;
  pn_memory_check(memory, name_p, 1);

  uint32_t name_len = pn_memory_check_cstr(executor->memory, name_p);
  PN_CHECK(name_len > 0);

#define PN_WRITE_BUILTIN(offset, name)                           \
  pn_memory_write_u32(memory, table + offset * sizeof(uint32_t), \
                      pn_builtin_to_pointer(PN_BUILTIN_##name));

  const char* iface_name = memory->data + name_p;
  PN_TRACE(IRT, "    NACL_IRT_QUERY(%u (%s), %u, %u)\n", name_p, iface_name,
           table, table_size);

  if (strcmp(iface_name, "nacl-irt-basic-0.1") == 0) {
    PN_CHECK(table_size == 24);
    PN_WRITE_BUILTIN(0, NACL_IRT_BASIC_EXIT);
    PN_WRITE_BUILTIN(1, NACL_IRT_BASIC_GETTOD);
    PN_WRITE_BUILTIN(2, NACL_IRT_BASIC_CLOCK);
    PN_WRITE_BUILTIN(3, NACL_IRT_BASIC_NANOSLEEP);
    PN_WRITE_BUILTIN(4, NACL_IRT_BASIC_SCHED_YIELD);
    PN_WRITE_BUILTIN(5, NACL_IRT_BASIC_SYSCONF);
    return pn_executor_value_u32(24);
  } else if (strcmp(iface_name, "nacl-irt-dev-fdio-0.3") == 0) {
    PN_CHECK(table_size == 56);
    PN_WRITE_BUILTIN(0, NACL_IRT_FDIO_CLOSE);
    PN_WRITE_BUILTIN(1, NACL_IRT_FDIO_DUP);
    PN_WRITE_BUILTIN(2, NACL_IRT_FDIO_DUP2);
    PN_WRITE_BUILTIN(3, NACL_IRT_FDIO_READ);
    PN_WRITE_BUILTIN(4, NACL_IRT_FDIO_WRITE);
    PN_WRITE_BUILTIN(5, NACL_IRT_FDIO_SEEK);
    PN_WRITE_BUILTIN(6, NACL_IRT_FDIO_FSTAT);
    PN_WRITE_BUILTIN(7, NACL_IRT_FDIO_GETDENTS);
    PN_WRITE_BUILTIN(8, NACL_IRT_FDIO_FCHDIR);
    PN_WRITE_BUILTIN(9, NACL_IRT_FDIO_FCHMOD);
    PN_WRITE_BUILTIN(10, NACL_IRT_FDIO_FSYNC);
    PN_WRITE_BUILTIN(11, NACL_IRT_FDIO_FDATASYNC);
    PN_WRITE_BUILTIN(12, NACL_IRT_FDIO_FTRUNCATE);
    PN_WRITE_BUILTIN(13, NACL_IRT_FDIO_ISATTY);
    return pn_executor_value_u32(56);
  } else if (strcmp(iface_name, "nacl-irt-fdio-0.1") == 0) {
    PN_CHECK(table_size == 32);
    PN_WRITE_BUILTIN(0, NACL_IRT_FDIO_CLOSE);
    PN_WRITE_BUILTIN(1, NACL_IRT_FDIO_DUP);
    PN_WRITE_BUILTIN(2, NACL_IRT_FDIO_DUP2);
    PN_WRITE_BUILTIN(3, NACL_IRT_FDIO_READ);
    PN_WRITE_BUILTIN(4, NACL_IRT_FDIO_WRITE);
    PN_WRITE_BUILTIN(5, NACL_IRT_FDIO_SEEK);
    PN_WRITE_BUILTIN(6, NACL_IRT_FDIO_FSTAT);
    PN_WRITE_BUILTIN(7, NACL_IRT_FDIO_GETDENTS);
    return pn_executor_value_u32(32);
  } else if (strcmp(iface_name, "nacl-irt-dev-filename-0.3") == 0) {
    PN_CHECK(table_size == 64);
    PN_WRITE_BUILTIN(0, NACL_IRT_FILENAME_OPEN);
    PN_WRITE_BUILTIN(1, NACL_IRT_FILENAME_STAT);
    PN_WRITE_BUILTIN(2, NACL_IRT_FILENAME_MKDIR);
    PN_WRITE_BUILTIN(3, NACL_IRT_FILENAME_RMDIR);
    PN_WRITE_BUILTIN(4, NACL_IRT_FILENAME_CHDIR);
    PN_WRITE_BUILTIN(5, NACL_IRT_FILENAME_GETCWD);
    PN_WRITE_BUILTIN(6, NACL_IRT_FILENAME_UNLINK);
    PN_WRITE_BUILTIN(7, NACL_IRT_FILENAME_TRUNCATE);
    PN_WRITE_BUILTIN(8, NACL_IRT_FILENAME_LSTAT);
    PN_WRITE_BUILTIN(9, NACL_IRT_FILENAME_LINK);
    PN_WRITE_BUILTIN(10, NACL_IRT_FILENAME_RENAME);
    PN_WRITE_BUILTIN(11, NACL_IRT_FILENAME_SYMLINK);
    PN_WRITE_BUILTIN(12, NACL_IRT_FILENAME_CHMOD);
    PN_WRITE_BUILTIN(13, NACL_IRT_FILENAME_ACCESS);
    PN_WRITE_BUILTIN(14, NACL_IRT_FILENAME_READLINK);
    PN_WRITE_BUILTIN(15, NACL_IRT_FILENAME_UTIMES);
    return pn_executor_value_u32(64);
  } else if (strcmp(iface_name, "nacl-irt-memory-0.3") == 0) {
    PN_CHECK(table_size == 12);
    PN_WRITE_BUILTIN(0, NACL_IRT_MEMORY_MMAP);
    PN_WRITE_BUILTIN(1, NACL_IRT_MEMORY_MUNMAP);
    PN_WRITE_BUILTIN(2, NACL_IRT_MEMORY_MPROTECT);
    return pn_executor_value_u32(12);
  } else if (strcmp(iface_name, "nacl-irt-tls-0.1") == 0) {
    PN_CHECK(table_size == 8);
    PN_WRITE_BUILTIN(0, NACL_IRT_TLS_INIT);
    PN_WRITE_BUILTIN(1, NACL_IRT_TLS_GET);
    return pn_executor_value_u32(8);
  } else if (strcmp(iface_name, "nacl-irt-thread-0.1") == 0) {
    PN_CHECK(table_size == 12);
    PN_WRITE_BUILTIN(0, NACL_IRT_THREAD_CREATE);
    PN_WRITE_BUILTIN(1, NACL_IRT_THREAD_EXIT);
    PN_WRITE_BUILTIN(2, NACL_IRT_THREAD_NICE);
    return pn_executor_value_u32(12);
  } else if (strcmp(iface_name, "nacl-irt-futex-0.1") == 0) {
    PN_CHECK(table_size == 8);
    PN_WRITE_BUILTIN(0, NACL_IRT_FUTEX_WAIT_ABS);
    PN_WRITE_BUILTIN(1, NACL_IRT_FUTEX_WAKE);
    return pn_executor_value_u32(8);
  } else if (strcmp(iface_name, "nacl-irt-ppapihook-0.1") == 0) {
#if PN_PPAPI
    if (g_pn_ppapi) {
      PN_CHECK(table_size == 8);
      PN_WRITE_BUILTIN(0, NACL_IRT_PPAPIHOOK_PPAPI_START);
      PN_WRITE_BUILTIN(1, NACL_IRT_PPAPIHOOK_PPAPI_REGISTER_THREAD_CREATOR);
      return pn_executor_value_u32(8);
    } else
#endif /* PN_PPAPI */
      return pn_executor_value_u32(0);
  } else {
    PN_TRACE(IRT, "Unknown interface name: \"%s\".\n", iface_name);
    return pn_executor_value_u32(0);
  }

#undef PN_WRITE_BUILTIN
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_EXIT(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(exit_code, 0, i32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_EXIT(%d)\n", exit_code);
  executor->exit_code = exit_code;
  executor->exiting = PN_TRUE;
  thread->state = PN_THREAD_DEAD;
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_GETTOD(PNThread* thread,
                                                       PNFunction* function,
                                                       uint32_t num_args,
                                                       PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(tv_p, 0, i32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_GETTOD(%u)\n", tv_p);
  struct timeval tv;
  gettimeofday(&tv, NULL);

  /*
  * offset size
  * 0  8 time_t      tv_sec;
  * 8  4 suseconds_t tv_usec;
  * 16 total
  */
  pn_memory_write_u64(executor->memory, tv_p + 0, tv.tv_sec);
  pn_memory_write_u32(executor->memory, tv_p + 8, tv.tv_usec);
  PN_TRACE(IRT, "      tv_sec => %" PRId64 " tv_usec = %u\n",
           (int64_t)tv.tv_sec, (int32_t)tv.tv_usec);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_SYSCONF(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(name, 0, u32);
  PN_BUILTIN_ARG(value_p, 1, u32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_SYSCONF(%u, %u)\n", name, value_p);
  switch (name) {
    case 2: /* _SC_PAGESIZE */
      pn_memory_write_u32(executor->memory, value_p, PN_PAGESIZE);
      PN_TRACE(IRT, "      SC_PAGESIZE => %u\n", PN_PAGESIZE);
      break;
    default:
      return pn_executor_value_u32(PN_EINVAL);
  }
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_CLOSE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(fd, 0, u32);
  return pn_executor_value_u32(pn_filesystem_fdio_close(executor, fd));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_READ(PNThread* thread,
                                                    PNFunction* function,
                                                    uint32_t num_args,
                                                    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nread_p, 3, u32);
  return pn_executor_value_u32(
      pn_filesystem_fdio_read(executor, fd, buf_p, count, nread_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_SEEK(PNThread* thread,
                                                    PNFunction* function,
                                                    uint32_t num_args,
                                                    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(offset, 1, u32);
  PN_BUILTIN_ARG(whence, 2, u32);
  PN_BUILTIN_ARG(new_offset_p, 3, u32);
  return pn_executor_value_u32(
      pn_filesystem_fdio_seek(executor, fd, offset, whence, new_offset_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_WRITE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nwrote_p, 3, u32);
  return pn_executor_value_u32(
      pn_filesystem_fdio_write(executor, fd, buf_p, count, nwrote_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_FSTAT(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(stat_p, 1, u32);
  return pn_executor_value_u32(pn_filesystem_fdio_fstat(executor, fd, stat_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_ISATTY(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(result_p, 1, u32);
  return pn_executor_value_u32(
      pn_filesystem_fdio_isatty(executor, fd, result_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_GETCWD(PNThread* thread,
                                                          PNFunction* function,
                                                          uint32_t num_args,
                                                          PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);
  return pn_executor_value_u32(
      pn_filesystem_filename_getcwd(executor, pathname_p, len));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_OPEN(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(oflag, 1, u32);
  PN_BUILTIN_ARG(cmode, 2, u32);
  PN_BUILTIN_ARG(newfd_p, 3, u32);
  return pn_executor_value_u32(
      pn_filesystem_filename_open(executor, pathname_p, oflag, cmode, newfd_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_ACCESS(PNThread* thread,
                                                          PNFunction* function,
                                                          uint32_t num_args,
                                                          PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(amode, 1, u32);
  return pn_executor_value_u32(
      pn_filesystem_filename_access(executor, pathname_p, amode));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_READLINK(
    PNThread* thread,
    PNFunction* function,
    uint32_t num_args,
    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(path_p, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nread_p, 3, u32);
  return pn_executor_value_u32(
      pn_filesystem_filename_readlink(executor, path_p, buf_p, count, nread_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_STAT(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(stat_p, 1, u32);
  return pn_executor_value_u32(
      pn_filesystem_filename_stat(executor, pathname_p, stat_p));
}

static PNRuntimeValue pn_builtin_NACL_IRT_MEMORY_MMAP(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 6);
  PN_BUILTIN_ARG(addr_pp, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);
  PN_BUILTIN_ARG(prot, 2, u32);
  PN_BUILTIN_ARG(flags, 3, u32);
  PN_BUILTIN_ARG(fd, 4, i32);
  PN_BUILTIN_ARG(off, 5, u64);
  PN_TRACE(IRT, "    NACL_IRT_MEMORY_MMAP(%u, %u, %u, %u, %d, %" PRId64 ")\n",
           addr_pp, len, prot, flags, fd, off);

  if ((flags & 0x20) != 0x20) { /* MAP_ANONYMOUS */
    PN_TRACE(IRT, "      not anonymous, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  /* TODO(binji): optimize */
  PNMemory* memory = executor->memory;
  assert(pn_is_aligned(executor->heap_end, PN_PAGESIZE));
  len = pn_align_up(len, PN_PAGESIZE);
  uint32_t pages = len >> PN_PAGESHIFT;
  uint32_t first_page = memory->heap_start >> PN_PAGESHIFT;
  uint32_t last_page = executor->heap_end >> PN_PAGESHIFT;
  uint32_t consecutive = 0;
  uint32_t result;
  uint32_t page_start = 0;
  uint32_t new_heap_end;
  uint32_t i;
  PN_TRACE(IRT, "      Searching from [%d, %d)\n", first_page, last_page);
  for (i = first_page; i < last_page; ++i) {
    if (!pn_bitset_is_set(&executor->mapped_pages, i)) {
      if (consecutive == 0) {
        PN_TRACE(IRT, "      %d unmapped. starting. %d pages needed.\n", i,
                 pages);
        page_start = i;
      }
      if (++consecutive == pages) {
        /* Found a spot */
        PN_TRACE(IRT, "      found %d consecutive pages.\n", pages);
        result = page_start << PN_PAGESHIFT;
        goto found;
      }
    } else {
      if (consecutive > 0) {
        PN_TRACE(IRT, "      %d mapped.\n", i);
      }
      consecutive = 0;
    }
  }

  /* Move heap_end back, if possible */
  result = executor->heap_end;
  assert(pn_is_aligned(result, PN_PAGESIZE));
  pn_memory_check(memory, result, len);
  new_heap_end = executor->heap_end + len;
  if (new_heap_end > executor->main_thread->current_frame->memory_stack_top) {
    PN_FATAL("Out of heap\n");
  }
  executor->heap_end = new_heap_end;

found:
  for (i = 0; i < pages; ++i) {
    pn_bitset_set(&executor->mapped_pages, (result >> PN_PAGESHIFT) + i,
                  PN_TRUE);
  }
  pn_memory_write_u32(memory, addr_pp, result);
  PN_TRACE(IRT, "      returning %u, errno = 0\n", result);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_MEMORY_MUNMAP(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);

  PNMemory* memory = executor->memory;
  uint32_t old_addr_p = addr_p;
  addr_p = pn_align_down(addr_p, PN_PAGESIZE);
  len = pn_align_up(old_addr_p + len, PN_PAGESIZE) - addr_p;
  uint32_t pages = len >> PN_PAGESHIFT;
  uint32_t first_page = memory->heap_start >> PN_PAGESHIFT;
  uint32_t last_page = executor->heap_end >> PN_PAGESHIFT;
  uint32_t begin = addr_p >> PN_PAGESHIFT;
  uint32_t end = begin + pages;
  if (begin < first_page) {
    begin = first_page;
  }
  if (end > last_page) {
    end = last_page;
  }
  uint32_t i;
  for (i = begin; i < end; ++i) {
    pn_bitset_set(&executor->mapped_pages, i, PN_FALSE);
  }

  PN_TRACE(IRT, "    NACL_IRT_MEMORY_MUNMAP(%u, %u)\n", addr_p, len);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_TLS_INIT(PNThread* thread,
                                                   PNFunction* function,
                                                   uint32_t num_args,
                                                   PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(thread_ptr_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_TLS_INIT(%u)\n", thread_ptr_p);
  /* How big is TLS? */
  pn_memory_check(executor->memory, thread_ptr_p, 1);
  thread->tls = thread_ptr_p;
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FUTEX_WAIT_ABS(PNThread* thread,
                                                         PNFunction* function,
                                                         uint32_t num_args,
                                                         PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  /* (Doc From irt.h)
  If |*addr| still contains |value|, futex_wait_abs() waits to be woken up by a
  futex_wake(addr,...) call from another thread; otherwise, it immediately
  returns EAGAIN (which is the same as EWOULDBLOCK).  If woken by another
  thread, it returns 0.  If |abstime| is non-NULL and the time specified by
  |*abstime| passes, this returns ETIMEDOUT.
  */
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(value, 1, u32);
  PN_BUILTIN_ARG(abstime_p, 2, u32);
  switch (thread->futex_state) {
    case PN_FUTEX_NONE: {
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      uint32_t read = pn_memory_read_u32(executor->memory, addr_p);
      if (read != value) {
        return pn_executor_value_u32(PN_EAGAIN);
      }

      thread->wait_addr = addr_p;

      if (abstime_p != 0) {
        thread->has_timeout = PN_TRUE;
        thread->timeout_sec = pn_memory_read_u64(executor->memory, abstime_p);
        /* IRT timeout is a timespec, which uses nanoseconds. Convert to
         * microseconds for comparison with gettimeofday */
        thread->timeout_usec =
            pn_memory_read_u32(executor->memory, abstime_p + 8) /
            PN_NANOSECONDS_IN_A_MICROSECOND;
      } else {
        thread->has_timeout = PN_FALSE;
        thread->timeout_sec = 0;
        thread->timeout_usec = 0;
      }

      thread->state = PN_THREAD_BLOCKED;

      /* Return an arbitrary value. We'll set the real value when the thread is
       * unblocked */
      return pn_executor_value_u32(0);
    }

    case PN_FUTEX_TIMEDOUT:
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      PN_TRACE(IRT, "      errno = ETIMEDOUT (%d)\n", PN_ETIMEDOUT);
      thread->futex_state = PN_FUTEX_NONE;
      return pn_executor_value_u32(PN_ETIMEDOUT);

    case PN_FUTEX_WOKEN:
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      PN_TRACE(IRT, "      errno = 0\n");
      thread->futex_state = PN_FUTEX_NONE;
      return pn_executor_value_u32(0);

    default:
      PN_UNREACHABLE();
      return pn_executor_value_u32(PN_EINVAL);
  }
}

static PNRuntimeValue pn_builtin_NACL_IRT_FUTEX_WAKE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  /*
  (Doc From irt.h)
  futex_wake() wakes up threads that are waiting on |addr| using futex_wait().
  |nwake| is the maximum number of threads that will be woken up.  The number
  of threads that were woken is returned in |*count|.
  */
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(nwake, 1, u32);
  PN_BUILTIN_ARG(count_p, 2, u32);
  PN_TRACE(IRT, "    NACL_IRT_WAKE(%u, %u, %u)\n", addr_p, nwake, count_p);

  uint32_t woken = 0;
  PNThread* i;
  for (i = thread->next; i != thread && woken < nwake; i = i->next) {
    if (i->state == PN_THREAD_BLOCKED && i->wait_addr == addr_p) {
      PN_TRACE(IRT, "      waking thread %d\n", i->id);
      i->state = PN_THREAD_RUNNING;
      i->futex_state = PN_FUTEX_WOKEN;
      woken++;
    }
  }

  pn_memory_write_u32(executor->memory, count_p, woken);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_THREAD_CREATE(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(start_func_p, 0, u32);
  PN_BUILTIN_ARG(stack_p, 1, u32);
  PN_BUILTIN_ARG(thread_p, 2, u32);
  PN_TRACE(IRT, "    NACL_IRT_THREAD_CREATE(%u, %u, %u)\n", start_func_p,
           stack_p, thread_p);
  PNThread* new_thread;
  if (executor->dead_threads) {
    /* Dead thread list is singly-linked */
    new_thread = executor->dead_threads;
    executor->dead_threads = new_thread->next;
  } else {
    new_thread = pn_allocator_alloc(&executor->allocator, sizeof(PNThread),
                                    PN_DEFAULT_ALIGN);
  }

  pn_allocator_init(&new_thread->allocator, PN_MIN_CHUNKSIZE, "thread");
  new_thread->executor = executor;
  new_thread->current_frame = &executor->sentinel_frame;
  new_thread->tls = thread_p;
  new_thread->id = executor->next_thread_id++;
  new_thread->state = PN_THREAD_RUNNING;
  new_thread->futex_state = PN_FUTEX_NONE;
  new_thread->module = thread->executor->module;
  new_thread->next = thread->next;
  new_thread->prev = thread;
  thread->next->prev = new_thread;
  thread->next = new_thread;

  PNFunctionId new_function_id = pn_function_pointer_to_index(start_func_p);
  PN_CHECK(new_function_id >= PN_MAX_BUILTINS);
  new_function_id -= PN_MAX_BUILTINS;
  pn_function_id_check(executor->module, new_function_id);
  PNFunction* new_function = &executor->module->functions[new_function_id];
  pn_thread_push_function(new_thread, new_function_id, new_function);
  new_thread->current_frame->memory_stack_top = stack_p;

  PN_TRACE(IRT, "      created thread %d\n", new_thread->id);

  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_THREAD_EXIT(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(stack_flag_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_THREAD_EXIT(%u)\n", stack_flag_p);
  PN_CHECK(thread != executor->main_thread);
  thread->state = PN_THREAD_DEAD;

  if (stack_flag_p) {
    pn_memory_write_u32(executor->memory, stack_flag_p, 0);
  }

  return pn_executor_value_u32(0);
}

#if PN_PPAPI
static PNEvent* pn_event_allocate(PNPpapi* ppapi);
static void pn_event_enqueue(PNPpapi* ppapi, PNEvent* event);

static PNRuntimeValue pn_builtin_NACL_IRT_PPAPIHOOK_PPAPI_START(
    PNThread* thread,
    PNFunction* function,
    uint32_t num_args,
    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(start_functions_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_PPAPIHOOK_PPAPI_START(%u)\n", start_functions_p);

  uint32_t initialize_func =
      pn_memory_read_u32(executor->memory, start_functions_p);
  executor->ppapi.shutdown_func =
      pn_memory_read_u32(executor->memory, start_functions_p + 4);
  executor->ppapi.get_interface_func =
      pn_memory_read_u32(executor->memory, start_functions_p + 8);

  /* Replace the main thread (which should be this thread) with the event loop
   * thread */
  PN_CHECK(thread == &executor->start_thread);
  PN_CHECK(thread == executor->main_thread);

  PNThread* new_thread = &executor->ppapi.event_thread;
  new_thread->executor = executor;
  new_thread->current_frame = &executor->sentinel_frame;
  new_thread->tls = thread->tls;
  new_thread->id = thread->id;
  new_thread->state = PN_THREAD_RUNNING;
  new_thread->futex_state = PN_FUTEX_NONE;
  new_thread->module = thread->executor->module;
  new_thread->next = thread->next;
  new_thread->prev = thread->prev;
  thread->next->prev = new_thread;
  thread->prev->next = new_thread;

  /* Disconnect the start thread, and mark it as being in the event loop so it
   * doesn't keep running. Set next to new_thread so the event thread will be
   * scheduled next. */
  thread->next = new_thread;
  thread->prev = NULL;
  thread->state = PN_THREAD_EVENT_LOOP;

  executor->main_thread = new_thread;

  PNFunction* new_function =
      pn_thread_push_function_pointer(new_thread, initialize_func);
  PN_CHECK(new_function->num_args == 2);
  pn_thread_set_param_value(new_thread, 0, pn_executor_value_u32(PN_MODULE_ID));
  pn_thread_set_param_value(
      new_thread, 1, pn_executor_value_u32(
                         pn_builtin_to_pointer(PN_BUILTIN_PPB_GET_INTERFACE)));

  /* Use the same stack top as the main thread. This must be set after the new
   * function is pushed */
  new_thread->current_frame->memory_stack_top =
      thread->current_frame->memory_stack_top;

  /* Enqueue a did_change event to execute after initialization */
  PNEvent* event = pn_event_allocate(&executor->ppapi);
  event->type = PN_EVENT_TYPE_DID_CREATE;
  pn_event_enqueue(&executor->ppapi, event);

  return pn_executor_value_u32(0);
}

static PNRuntimeValue
pn_builtin_NACL_IRT_PPAPIHOOK_PPAPI_REGISTER_THREAD_CREATOR(
    PNThread* thread,
    PNFunction* function,
    uint32_t num_args,
    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(thread_functions_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_PPAPIHOOK_PPAPI_REGISTER_THREAD_CREATOR(%u)\n",
           thread_functions_p);

  /* TODO(binji): use to create audio thread...? */
  (void)executor;

  return pn_executor_value_u32(0);
}
#endif /* PN_PPAPI */

#define PN_BUILTIN_STUB(name)                                    \
  static PNRuntimeValue pn_builtin_##name(                       \
      PNThread* thread, PNFunction* function, uint32_t num_args, \
      PNValueId* arg_ids) {                                      \
    PN_TRACE(IRT, "    " #name "(...)\n");                       \
    return pn_executor_value_u32(PN_ENOSYS);                     \
  }

PN_BUILTIN_STUB(NACL_IRT_BASIC_CLOCK)
PN_BUILTIN_STUB(NACL_IRT_BASIC_NANOSLEEP)
PN_BUILTIN_STUB(NACL_IRT_BASIC_SCHED_YIELD)
PN_BUILTIN_STUB(NACL_IRT_FDIO_DUP)
PN_BUILTIN_STUB(NACL_IRT_FDIO_DUP2)
PN_BUILTIN_STUB(NACL_IRT_FDIO_GETDENTS)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FCHDIR)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FCHMOD)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FSYNC)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FDATASYNC)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FTRUNCATE)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_MKDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_RMDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_CHDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_UNLINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_TRUNCATE)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_LSTAT)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_LINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_RENAME)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_SYMLINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_CHMOD)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_UTIMES)
PN_BUILTIN_STUB(NACL_IRT_MEMORY_MPROTECT)
PN_BUILTIN_STUB(NACL_IRT_TLS_GET)
PN_BUILTIN_STUB(NACL_IRT_THREAD_NICE)

#undef PN_BUILTIN_STUB

#undef PN_BUILTIN_ARG

#endif /* PN_BUILTINS_H_ */
