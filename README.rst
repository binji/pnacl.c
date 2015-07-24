pnacl.c
=======

Read, parse and interpret Portable Native Client Executables (pexes).
See https://developers.google.com/native-client for more info about PNaCl.

Building
--------

To build debug (out/pnacl) and optimized builds (out/pnacl-opt-assert)::

  $ make

To build other various configurations::

  $ make everything

Tests
-----

To run tests::

  # Set your NACL_SDK_ROOT
  $ export NACL_SDK_ROOT=...
  $ make test

or run the Python test runner directly::

  # run all tests
  $ test/run-tests.py
  [+38|-0] (0.72s)

  # run tests that match a wildcard pattern
  $ test/run-tests.py bench -v
  . benchmark/binarytrees.txt (skipped)
  + benchmark/fannkuchredux.txt (0.035s)
  + benchmark/fasta.txt (0.034s)
  + benchmark/mandelbrot.txt (0.360s)
  . benchmark/meteor.txt (skipped)
  + benchmark/nbody.txt (0.036s)
  + benchmark/spectralnorm.txt (0.150s)
  [+5|-0] (0.62s)

  # run slow tests
  $ test/run-tests.py bench -v -s
  + benchmark/binarytrees.txt (1.395s)
  + benchmark/fannkuchredux.txt (0.041s)
  + benchmark/fasta.txt (0.033s)
  + benchmark/mandelbrot.txt (0.357s)
  + benchmark/meteor.txt (4.369s)
  + benchmark/nbody.txt (0.033s)
  + benchmark/spectralnorm.txt (0.142s)
  [+7|-0] (6.38s)

Benchmarks
----------

To run benchmarks::

  # Set your NACL_SDK_ROOT
  $ export NACL_SDK_ROOT=...
  $ test/run-benchmarks.py
  + benchmark/binarytrees.10.bench O0 (4.238s) (0.078s) (54.3x)
  + benchmark/binarytrees.10.bench O2 (4.238s) (0.070s) (60.4x)
  + benchmark/binarytrees.12.bench O0 (21.166s) (0.158s) (133.6x)
  + benchmark/binarytrees.12.bench O2 (21.166s) (0.116s) (182.4x)
  ...
  [+20|-0] (48.16s)

This runs the interpreter and compares it to a nexe translated using
pnacl-translate -O0 and -O2.
