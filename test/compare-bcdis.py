#!/usr/bin/env python
# Copyright 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import atexit
import difflib
import fnmatch
import itertools
import logging
import os
import re
import subprocess
import sys
import time


run_tests = __import__('run-tests')
Error = run_tests.Error

DIFF_QUANTUM = 100
NACL_SDK_ROOT = os.environ['NACL_SDK_ROOT']
NACL_CONFIG_PY = os.path.join(NACL_SDK_ROOT, 'tools', 'nacl_config.py')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT_DIR = os.path.dirname(SCRIPT_DIR)
DEFAULT_PNACL_EXE = os.path.join(REPO_ROOT_DIR, 'out', 'pnacl-opt-assert')
logger = logging.getLogger(__name__)


def RunNaClConfig(args):
  cmd = [sys.executable, NACL_CONFIG_PY] + args
  logging.info(' '.join(cmd))
  try:
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout, _ = process.communicate()
  except OSError as e:
    raise Error(str(e))
  return stdout.strip()


def LogBeginEnd(msg, f, *args):
  if logger.isEnabledFor(logging.INFO):
    sys.stderr.write(msg + '... ')
    sys.stderr.flush()
  result = f(*args)
  if logger.isEnabledFor(logging.INFO):
    sys.stderr.write('done.\n')
  return result


def PNaClBcdisLineIter(cmd):
  logging.info(' '.join(cmd))
  try:
    process = subprocess.Popen(cmd, bufsize=1, stdout=subprocess.PIPE)
    atexit.register(process.terminate)
    lines = iter(process.stdout.readline, '')
  except OSError as e:
    raise Error(str(e))

  def ReplaceNewlineWithNSpaces(n, s):
    return re.sub(r'\n\s*', ' ' * n, s)

  while True:
    line = next(lines)
    line = re.sub(r' <[@%]a\d+>', '', line)
    line = line.rstrip()

    # pnacl-bcdis bug?
    line = re.sub(r'i64 -0', 'i64 -9223372036854775808', line)

    # Join call splits
    if re.match(r'.*(?:%v\d+ = )?call', line):
      while ';' not in line:
        line = line + ' ' + next(lines).strip()

    # Join function definition splits
    elif re.match(r'.*function(?!:)', line):
      while '//' not in line:
        line = line + ' ' + next(lines).strip()
      # Always two spaces before the //
      line = re.sub(r'\s*//', '  //', line)

    # Join type definition splits
    elif re.match(r'.*@t\d+ ', line):
      while ';' not in line:
        line = line + ' ' + next(lines).strip()

    # Join function declaration splits
    elif re.match(r'.*(?:define|declare) (?:internal|external)', line):
      while ';' not in line:
        line = line + ' ' + next(lines).strip()

    if line:
      yield line + '\n'


def PNaClDotCLineIter(cmd):
  logging.info(' '.join(cmd))
  try:
    process = subprocess.Popen(cmd, bufsize=1, stderr=subprocess.PIPE)
    atexit.register(process.terminate)
    lines = iter(process.stderr.readline, '')
  except OSError as e:
    raise Error(str(e))

  while True:
    line = next(lines)
    line = line.rstrip()
    yield line + '\n'


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-e', '--executable', help='override executable.',
                      default=DEFAULT_PNACL_EXE)
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages. '
                      'Use more than once for more info.', action='count')
  parser.add_argument('-l', '--list', help='list all tests.',
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

  tests = run_tests.FindTestFiles(SCRIPT_DIR, '.pexe', pattern_re)
  if options.list:
    for test in tests:
      print test
    return 0

  if options.executable:
    if not os.path.exists(options.executable):
      parser.error('executable %s does not exist' % options.executable)

    options.executable = os.path.relpath(
        os.path.join(os.getcwd(), options.executable), SCRIPT_DIR)

  os.chdir(SCRIPT_DIR)

  pnacl_bcdis = RunNaClConfig(['-t', 'pnacl', '--tool', 'bcdis'])

  for test in tests:
    # Let's assume the files are mostly the same, but may be very large.
    # Compare in chunks of N lines.
    logging.warning('**** %s %s' % (test, '*' * (80 - 6 - len(test))))

    expected_cmd = [pnacl_bcdis, '--no-records', test]
    expected_name = ' '.join(expected_cmd)
    actual_cmd = [options.executable, '--trace-bcdis', '--no-dedupe-phi-nodes',
                  '-m 10m', test]
    actual_name = ' '.join(actual_cmd)

    expected_iter = PNaClBcdisLineIter(expected_cmd)
    actual_iter = PNaClDotCLineIter(actual_cmd)

    files_equal = True

    while True:
      expected_lines = list(itertools.islice(expected_iter, 0, DIFF_QUANTUM))
      actual_lines = list(itertools.islice(actual_iter, 0, DIFF_QUANTUM))
      if not expected_lines and not actual_lines:
        break

      diff_lines = list(difflib.unified_diff(
          expected_lines, actual_lines,
          fromfile=expected_name, tofile=actual_name))

      if diff_lines:
        logging.warning(''.join(diff_lines))
        files_equal = False
        break

    if files_equal:
      logging.warning('files do not differ')

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
