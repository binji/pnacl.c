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


class TestInfo(object):
  def __init__(self):
    self.name = ''
    self.header_lines = ''
    self.exe = '../out/pnacl'
    self.pexe = ''
    self.flags = []
    self.args = []
    self.expected_lines = ''
    self.expected_error = 0

  def _Error(self, msg):
    logger.error('- %s:\n  %s' % (self.name, msg))

  def Parse(self, filename):
    self.name = filename

    with open(filename) as f:
      lines = f.readlines()
      if len(lines) == 0:
        self._Error('empty test file')
        return False

      n = 0
      while n < len(lines):
        m = re.match(r'\s*#(.*)$', lines[n])
        if not m:
          break

        key, value = m.group(1).split(':')
        key = key.strip()
        value = value.strip()
        if key.lower() == 'exe':
          if self.exe:
            self._Error('exe already set')
            return False
          self.exe = value
        elif key.lower() == 'flags':
          if self.flags:
            self._Error('flags already set')
            return False
          self.flags = shlex.split(value)
        elif key.lower() == 'file':
          if self.pexe:
            self._Error('pexe already set')
            return False
          self.pexe = value
        elif key.lower() == 'error':
          if self.expected_error:
            self._Error('error already set')
            return False
          self.expected_error = int(value)
        elif key.lower() == 'args':
          if self.args:
            self._Error('args already set')
            return False
          self.args = shlex.split(value)
        n += 1

      self.header_lines = lines[:n]
      self.expected_lines = lines[n:]
    return True

  def Run(self, options):
    cmd = [self.exe]
    if self.flags:
      cmd += self.flags
    if self.pexe:
      cmd += [self.pexe]
    if self.args:
      cmd += ['--'] + self.args
    try:
      process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
      stdout, stderr = process.communicate()
    except OSError as e:
      self._Error('%s: %s' % (' '.join(cmd), str(e)))
      return False

    if process.returncode != self.expected_error:
      self._Error('%s: expected error code %d, got %d.' % (
          ' '.join(cmd), self.expected_error, process.returncode))
      return False

    stderr_lines = stderr.splitlines(1)

    if options.rebase:
      with open(self.name, 'w') as f:
        f.writelines(self.header_lines + stderr_lines)
      return True
    else:
      matched = True
      if len(stderr_lines) == len(self.expected_lines):
        num_lines = len(self.expected_lines)
        for n in range(num_lines):
          if stderr_lines[n] != self.expected_lines[n]:
            matched = False
      else:
        matched = False

      if matched:
        if logger.isEnabledFor(logging.INFO):
          logger.info('+ %s' % self.name)
      else:
        if logger.isEnabledFor(logging.INFO):
          diff = ''.join(
              list(difflib.unified_diff(self.expected_lines, stderr_lines,
                                        fromfile='expected', tofile='actual')))
          logger.info('- %s:\n%s' % (self.name, diff))
        else:
          logger.error('- %s failed.' % self.name)

      logger.debug('    cmd = %s' % ' '.join(cmd))

      return matched


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
    pattern_re = '|'.join(fnmatch.translate(p) for p in options.patterns)
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

  os.chdir(SCRIPT_DIR)

  passed_tests = []
  failed_tests = []
  for test in tests:
    if not re.match(pattern_re, test):
      continue

    info = TestInfo()
    if info.Parse(test) and info.Run(options):
      passed_tests.append(info)
    else:
      failed_tests.append(info)

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
