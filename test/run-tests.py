#!/usr/bin/env python
# Copyright 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import fnmatch
import multiprocessing
import os
import Queue
import re
import shlex
import subprocess
import sys
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT_DIR = os.path.dirname(SCRIPT_DIR)
DEFAULT_PNACL_EXE = os.path.join(REPO_ROOT_DIR, 'out', 'pnacl-opt-assert')


class Error(Exception):
  pass


def AsList(value):
  if value is None:
    return []
  elif type(value) is list:
    return value
  else:
    return [value]


def Indent(s, spaces):
  return ''.join(' '*spaces + l for l in s.splitlines(1))


def DiffLines(expected, actual):
  expected_lines = expected.splitlines(1)
  actual_lines = actual.splitlines(1)
  return list(difflib.unified_diff(expected_lines, actual_lines,
                                   fromfile='expected', tofile='actual'))


def FindTestFiles(directory, ext, filter_pattern_re):
  tests = []
  for root, dirs, files in os.walk(directory):
    for f in files:
      path = os.path.join(root, f)
      if os.path.splitext(f)[1] == ext:
        tests.append(os.path.relpath(path, SCRIPT_DIR))
  tests.sort()
  return [test for test in tests if re.match(filter_pattern_re, test)]


class TestInfo(object):
  def __init__(self):
    self.name = ''
    self.header_lines = []
    self.stdout_file = None
    self.expected_stdout = ''
    self.expected_stderr = ''
    self.exe = None
    self.pexe = ''
    self.flags = []
    self.args = []
    self.expected_error = 0
    self.slow = False

  def Parse(self, filename):
    self.name = filename

    with open(filename) as f:
      seen_keys = set()
      state = 'header'
      empty = True
      header_lines = []
      stdout_lines = []
      stderr_lines = []
      for line in f.readlines():
        empty = False
        m = re.match(r'\s*#(.*)$', line)
        if m:
          if state == 'stdout':
            raise Error('unexpected directive in STDOUT block: %s' % line)

          directive = m.group(1).strip()
          if directive.lower() == 'stdout:':
            if 'stdout_file' in seen_keys:
              raise Error('can\'t have stdout section and stdout file')
            state = 'stdout'
            continue

          if state != 'header':
            raise Error('unexpected directive: %s' % line)

          key, value = directive.split(':')
          key = key.strip().lower()
          value = value.strip()
          if key in seen_keys:
            raise Error('%s already set' % key)
          seen_keys.add(key)
          if key == 'exe':
            self.exe = value
          elif key == 'flags':
            self.flags = shlex.split(value)
          elif key == 'file':
            self.pexe = value
          elif key == 'error':
            self.expected_error = int(value)
          elif key == 'args':
            self.args = shlex.split(value)
          elif key == 'stdout_file':
            self.stdout_file = value
            with open(self.stdout_file) as s:
              self.expected_stdout = s.read()
          elif key == 'slow':
            self.slow = True
          else:
            raise Error('Unknown directive: %s' % key)
        elif state == 'header':
          state = 'stderr'

        if state == 'header':
          header_lines.append(line)
        elif state == 'stderr':
          stderr_lines.append(line)
        elif state == 'stdout':
          stdout_lines.append(line)
    if empty:
      raise Error('empty test file')

    self.header = ''.join(header_lines)
    if not self.stdout_file:
      self.expected_stdout = ''.join(stdout_lines)
    self.expected_stderr = ''.join(stderr_lines)

  def GetExecutable(self, override_exe):
    if override_exe:
      exe = override_exe
    elif self.exe:
      exe = os.path.join(REPO_ROOT_DIR, self.exe)
    else:
      exe = DEFAULT_PNACL_EXE

    return os.path.relpath(exe, SCRIPT_DIR)

  def GetCommand(self, override_exe=None):
    cmd = [self.GetExecutable(override_exe)]
    cmd += self.flags
    cmd += AsList(self.pexe)
    cmd += ['--'] + AsList(self.args)
    return cmd

  def Run(self, override_exe=None):
    # Pass 'pnacl' as the executable name so the output is consistent
    cmd = ['pnacl'] + self.GetCommand(override_exe)[1:]
    exe = self.GetExecutable(override_exe)
    try:
      start_time = time.time()
      process = subprocess.Popen(cmd, executable=exe, stdout=subprocess.PIPE,
                                                      stderr=subprocess.PIPE)
      stdout, stderr = process.communicate()
      duration = time.time() - start_time
    except OSError as e:
      raise Error(str(e))

    return stdout, stderr, process.returncode, duration

  def Rebase(self, stdout, stderr):
    with open(self.name, 'w') as f:
      f.write(self.header)
      f.write(stderr)
      if self.stdout_file:
        with open(self.stdout_file, 'w') as s:
          s.write(stdout)
      elif stdout:
        f.write('# STDOUT:\n')
        f.write(stdout)

  def Diff(self, stdout, stderr):
    if self.expected_stderr != stderr:
      diff_lines = DiffLines(self.expected_stderr, stderr)
      raise Error('stderr mismatch:\n' + ''.join(diff_lines))

    if self.expected_stdout != stdout:
      if self.stdout_file:
        raise Error('stdout binary mismatch')
      else:
        diff_lines = DiffLines(self.expected_stdout, stdout)
        raise Error('stdout mismatch:\n' + ''.join(diff_lines))


class Status(object):
  def __init__(self, verbose):
    self.verbose = verbose
    self.start_time = None
    self.last_length = 0
    self.last_finished = None
    self.passed = 0
    self.failed = 0
    self.total = 0
    self.failed_tests = []

  def Start(self, total):
    self.total = total
    self.start_time = time.time()

  def Passed(self, info, duration):
    self.passed += 1
    if self.verbose:
      sys.stderr.write('+ %s (%.3fs)\n' % (info.name, duration))
    else:
      self.Clear()
      self._PrintShortStatus(info)
      sys.stderr.flush()

  def Failed(self, info, error_msg):
    self.failed += 1
    self.failed_tests.append(info)
    self.Clear()
    sys.stderr.write('- %s\n%s\n' % (info.name, Indent(error_msg, 2)))

  def Skipped(self, info):
    if self.verbose:
      sys.stderr.write('. %s (skipped)\n' % info.name)

  def Timeout(self):
    self._PrintShortStatus(self.last_finished)

  def Print(self):
    self._PrintShortStatus(None)
    sys.stderr.write('\n')

  def _PrintShortStatus(self, info):
    total_duration = time.time() - self.start_time
    name = info.name if info else ''
    percent = 100 * (self.passed + self.failed) / self.total
    status = '[+%d|-%d|%%%d] (%.2fs) %s\r' % (self.passed, self.failed,
                                              percent, total_duration, name)
    self.last_length = len(status)
    self.last_finished = info
    sys.stderr.write(status)

  def Clear(self):
    if not self.verbose:
      sys.stderr.write('%s\r' % (' ' * self.last_length))


def GetAllTestInfo(test_names, status):
  infos = []
  for test_name in test_names:
    info = TestInfo()
    try:
      info.Parse(test_name)
      infos.append(info)
    except Error as e:
      status.Failed(info, str(e))

  return infos


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-e', '--executable', help='override executable.')
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages.',
                      action='store_true')
  parser.add_argument('-l', '--list', help='list all tests.',
                      action='store_true')
  parser.add_argument('--list-exes',
                      help='list all executables needed for the tests.',
                      action='store_true')
  parser.add_argument('-r', '--rebase',
                      help='rebase a test to its current output.',
                      action='store_true')
  parser.add_argument('-s', '--slow', help='run slow tests.',
                      action='store_true')
  parser.add_argument('patterns', metavar='pattern', nargs='*',
                      help='test patterns.')
  options = parser.parse_args(args)

  if options.patterns:
    pattern_re = '|'.join(fnmatch.translate('*%s*' % p)
                          for p in options.patterns)
  else:
    pattern_re = '.*'

  test_names = FindTestFiles(SCRIPT_DIR, '.txt', pattern_re)
  if options.list:
    for test_name in test_names:
      print test_name
    return 0

  if options.executable:
    if not os.path.exists(options.executable):
      parser.error('executable %s does not exist' % options.executable)
    options.executable = os.path.abspath(options.executable)

  run_cwd = os.getcwd()
  os.chdir(SCRIPT_DIR)

  isatty = os.isatty(1)

  status = Status(options.verbose)
  infos = GetAllTestInfo(test_names, status)

  if options.list_exes:
    exes = set([info.exe for info in infos])
    if None in exes:
      exes.remove(None)
      exes.add(os.path.relpath(DEFAULT_PNACL_EXE, run_cwd))
    print '\n'.join(exes)
    return 0

  inq = multiprocessing.Queue()
  test_count = 0
  for info in infos:
    if not options.slow and info.slow:
      status.Skipped(info)
      continue
    inq.put(info)
    test_count += 1

  outq = multiprocessing.Queue()
  num_proc = multiprocessing.cpu_count()
  processes = []
  status.Start(test_count)

  def Worker(options, inq, outq):
    while True:
      try:
        info = inq.get(False)
        try:
          out = info.Run(options.executable)
        except Error as e:
          outq.put((info, e))
          continue
        outq.put((info, out))
      except Queue.Empty:
        break

  try:
    for p in range(num_proc):
      proc = multiprocessing.Process(target=Worker, args=(options, inq, outq))
      processes.append(proc)
      proc.start()

    finished_tests = 0
    while finished_tests < test_count:
      try:
        info, result = outq.get(True, 0.01)
      except Queue.Empty:
        status.Timeout()
        continue

      finished_tests += 1
      try:
        if isinstance(result, Error):
          raise result

        stdout, stderr, returncode, duration = result
        if returncode != info.expected_error:
          # This test has already failed, but diff it anyway.
          msg = 'expected error code %d, got %d.' % (info.expected_error,
                                                     returncode)
          try:
            info.Diff(stdout, stderr)
          except Error as e:
            msg += '\n' + str(e)
          raise Error(msg)
        else:
          if options.rebase:
            info.Rebase(stdout, stderr)
          else:
            info.Diff(stdout, stderr)
          status.Passed(info, duration)
      except Error as e:
        status.Failed(info, str(e))
  finally:
    for proc in processes:
      proc.terminate()
      proc.join()

  status.Clear()

  ret = 0

  if status.failed:
    sys.stderr.write('**** FAILED %s\n' % ('*' * (80 - 14)))
    for info in status.failed_tests:
      name = info.name
      cmd = info.GetCommand(options.executable)
      exe = os.path.relpath(info.GetExecutable(options.executable), run_cwd)
      msg = Indent('cmd = (cd %s && %s)\n' % (
          os.path.relpath(SCRIPT_DIR, run_cwd), ' '.join(cmd)), 2)
      msg += Indent('rerun = %s\n' % ' '.join(
          [sys.executable, sys.argv[0], '-e', exe, name]), 2)
      sys.stderr.write('- %s\n%s\n' % (name, msg))
    ret = 1

  status.Print()
  return ret


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
