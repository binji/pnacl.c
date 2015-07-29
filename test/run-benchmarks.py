#!/usr/bin/env python
# Copyright 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import fnmatch
import logging
import os
import re
import shlex
import subprocess
import sys
import time

run_tests = __import__('run-tests')
Error = run_tests.Error
TestInfo = run_tests.TestInfo
AsList = run_tests.AsList


NACL_SDK_ROOT = os.environ['NACL_SDK_ROOT']
SEL_LDR_PY = os.path.join(NACL_SDK_ROOT, 'tools', 'sel_ldr.py')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BENCHMARK_DIR = os.path.join(SCRIPT_DIR, 'benchmark')
logger = logging.getLogger(__name__)


def RunNexe(test_info, override, suffix):
  no_ext = os.path.splitext(test_info.GetWithOverride('pexe', override))[0]
  nexe = no_ext + suffix + '.nexe'

  cmd = [sys.executable, SEL_LDR_PY, nexe]
  cmd += ['--'] + AsList(test_info.GetWithOverride('args', override))

  try:
    start_time = time.time()
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    duration = time.time() - start_time
  except OSError as e:
    raise Error(str(e))

  return stdout, stderr, duration


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-e', '--executable', help='override executable.',
                      default='out/pnacl')
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages. '
                      'Use more than once for more info.', action='count')
  parser.add_argument('-l', '--list', help='list all tests.',
                      action='store_true')
  parser.add_argument('-a', '--arch', help='nexe arch.', default='x86-64')
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

  tests = run_tests.FindTestFiles(BENCHMARK_DIR, '.bench', pattern_re)
  if options.list:
    for test in tests:
      print test
    return 0

  override = {}
  if options.executable:
    if not os.path.exists(options.executable):
      parser.error('executable %s does not exist' % options.executable)
    # We always run from SCRIPT_DIR, but allow the user to pass in executables
    # with a relative path from where they ran run-tests.py
    override['exe'] = os.path.relpath(
        os.path.join(os.getcwd(), options.executable), SCRIPT_DIR)

  os.chdir(SCRIPT_DIR)

  passed = 0
  failed = 0
  start_time = time.time()
  for test in tests:
    info = TestInfo()
    try:
      info.Parse(test)
      stdout, _, returncode, duration = info.Run(override)
      if returncode != info.expected_error:
        raise Error('expected error code %d, got %d.' % (info.expected_error,
                                                         returncode))
      for opt_level in (0, 2):
        suffix = '.O%d.' % opt_level + options.arch
        nexe_stdout, _, nexe_duration = RunNexe(info, override, suffix)
        if nexe_stdout != stdout:
          if info.stdout_file:
            raise Error('stdout binary mismatch')
          else:
            diff_lines = run_tests.DiffLines(nexe_stdout, stdout)
            raise Error('stdout mismatch:\n' + ''.join(diff_lines))
        percent = duration / nexe_duration
        logger.warning('+ %s O%d (%.3fs) (%.3fs) (%.1fx)' % (
            info.name, opt_level, duration, nexe_duration, percent))
        passed += 1
    except Error as e:
      failed += 1
      msg = run_tests.Indent(str(e), 2)
      logger.error('- %s\n%s' % (info.name, msg))

  run_tests.PrintStatus(passed, failed, start_time, False)

  if failed:
    return 1
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
