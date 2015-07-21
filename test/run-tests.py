#!/usr/bin/env python
# Copyright 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import cStringIO
import difflib
import fnmatch
import logging
import os
import re
import shlex
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT_DIR = os.path.dirname(SCRIPT_DIR)
logger = logging.getLogger(__name__)


class Error(Exception):
  pass


def LinesMatch(expected_lines, actual_lines):
  if len(actual_lines) != len(expected_lines):
    return False

  num_lines = len(expected_lines)
  for n in range(num_lines):
    if actual_lines[n] != expected_lines[n]:
      return False

  return True


def Indent(s, spaces):
  return ''.join(' '*spaces + l for l in s.splitlines(1))


def DiffLines(expected_lines, actual_lines):
  return list(difflib.unified_diff(expected_lines, actual_lines,
                                   fromfile='expected', tofile='actual'))


class TestInfo(object):
  def __init__(self):
    self.name = ''
    self.header_lines = []
    self.expected_stdout_lines = []
    self.expected_stderr_lines = []
    self.exe = '../out/pnacl'
    self.pexe = ''
    self.flags = []
    self.args = []
    self.expected_error = 0
    self.cmd = []

  def Parse(self, filename):
    self.name = filename

    with open(filename) as f:
      seen_keys = set()
      state = 'header'
      empty = True
      for line in f.readlines():
        empty = False
        m = re.match(r'\s*#(.*)$', line)
        if m:
          if state == 'stdout':
            raise Error('unexpected directive in STDOUT block: %s' % line)

          directive = m.group(1).strip()
          if directive.lower() == 'stdout:':
            state = 'stdout'
            continue

          if state != 'header':
            raise Error('unexpected directive: %s' % line)

          key, value = directive.split(':')
          key = key.strip()
          value = value.strip()
          if key in seen_keys:
            raise Error('%s already set' % key)
          seen_keys.add(key)
          if key.lower() == 'exe':
            self.exe = value
          elif key.lower() == 'flags':
            self.flags = shlex.split(value)
          elif key.lower() == 'file':
            self.pexe = value
          elif key.lower() == 'error':
            self.expected_error = int(value)
          elif key.lower() == 'args':
            self.args = shlex.split(value)
          else:
            raise Error('Unknown directive: %s' % key)
        elif state == 'header':
          state = 'stderr'

        if state == 'header':
          self.header_lines.append(line)
        elif state == 'stderr':
          self.expected_stderr_lines.append(line)
        elif state == 'stdout':
          self.expected_stdout_lines.append(line)
    if empty:
      raise Error('empty test file')

  def Run(self):
    self.cmd = [self.exe]
    if self.flags:
      self.cmd += self.flags
    if self.pexe:
      self.cmd += [self.pexe]
    if self.args:
      self.cmd += ['--'] + self.args
    try:
      process = subprocess.Popen(self.cmd, stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE)
      stdout, stderr = process.communicate()
    except OSError as e:
      raise Error(str(e))

    if process.returncode != self.expected_error:
      raise Error('expected error code %d, got %d.' % (self.expected_error,
                                                       process.returncode))

    return stdout.splitlines(1), stderr.splitlines(1)

  def Rebase(self, stdout_lines, stderr_lines):
    with open(self.name, 'w') as f:
      f.writelines(self.header_lines)
      f.writelines(stderr_lines)
      f.write('# STDOUT:\n')
      f.writelines(stdout_lines)


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages. '
                      'Use more than once for more info.', action='count')
  parser.add_argument('-l', '--list', help='list all tests.',
                      action='store_true')
  parser.add_argument('-r', '--rebase',
                      help='rebase a test to its current output.',
                      action='store_true')
  parser.add_argument('patterns', metavar='pattern', nargs='*',
                      help='test patterns.')
  options = parser.parse_args(args)

  if options.patterns:
    pattern_re = '|'.join(fnmatch.translate('*%s*' % p)
                          for p in options.patterns)
  else:
    pattern_re = '.*'

  if options.verbose >= 2:
    level=logging.DEBUG
  elif options.verbose:
    level=logging.INFO
  else:
    level=logging.WARNING
  logging.basicConfig(level=level, format='%(message)s')

  tests = []
  for root, dirs, files in os.walk(SCRIPT_DIR):
    for f in files:
      path = os.path.join(root, f)
      if os.path.splitext(f)[1] == '.txt':
        tests.append(os.path.relpath(path, SCRIPT_DIR))

  if options.list:
    for test in tests:
      print test
    return 1

  os.chdir(SCRIPT_DIR)

  passed_tests = []
  failed_tests = []
  for test in tests:
    if not re.match(pattern_re, test):
      continue

    info = TestInfo()
    try:
      info.Parse(test)
      stdout_lines, stderr_lines = info.Run()
      if options.rebase:
        info.Rebase(stdout_lines, stderr_lines)
      else:
        stderr_matched = LinesMatch(info.expected_stderr_lines, stderr_lines)
        if not stderr_matched:
          diff_lines = DiffLines(info.expected_stderr_lines, stderr_lines)
          raise Error('stderr mismatch:\n' + ''.join(diff_lines))

        stdout_matched = LinesMatch(info.expected_stdout_lines, stdout_lines)
        if not stdout_matched:
          diff_lines = DiffLines(info.expected_stdout_lines, stdout_lines)
          raise Error('stdout mismatch:\n' + ''.join(diff_lines))

      passed_tests.append(info)
      logger.info('+ %s' % info.name)
    except Error as e:
      failed_tests.append(info)
      msg = ''
      if logger.isEnabledFor(logging.DEBUG) and info.cmd:
        msg += Indent('cmd = %s\n' % ' '.join(info.cmd), 2)
      msg += Indent(str(e), 2)
      logger.error('- %s\n%s' % (info.name, msg))

  logger.warning('[+%d|-%d]' % (len(passed_tests), len(failed_tests)))
  if failed_tests:
    return 1
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
