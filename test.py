#!/usr/bin/env python
# vim:fileencoding=utf-8
# License: GPLv3 Copyright: 2017, Kovid Goyal <kovid at kovidgoyal.net>

from __future__ import (absolute_import, division, print_function,
                        unicode_literals)

import bz2
import os
import subprocess
import sys
import unittest

base = os.path.dirname(os.path.abspath(__file__))
iswindows = hasattr(sys, 'getwindowsversion')


def run(input_data,
        query,
        threads=1,
        mark=False,
        level1=None,
        level2=None,
        level3=None):
    if isinstance(input_data, (list, tuple)):
        input_data = '\n'.join(input_data)
    if not isinstance(input_data, bytes):
        input_data = input_data.encode('utf-8')
    exe = 'subseq-matcher-debug'
    if iswindows:
        exe = 'subseq-matcher.exe'
    cmd = [
        os.path.join(base, 'build', exe), '-t',
        str(threads)
    ]
    if mark:
        cmd.extend(['-b', r'\e[32m', '-a', r'\e[39m'])
    for i in '123':
        val = locals()['level' + i]
        if val is not None:
            cmd.extend(['-' + i, val])
    cmd.append(query)
    p = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    stdout = p.communicate(input_data)[0]
    return p.wait(), stdout.decode('utf-8').splitlines()


class TestMatcher(unittest.TestCase):
    def run_matcher(self, *args, **kwargs):
        rc, result = run(*args, **kwargs)
        self.assertEqual(rc, 0, '\n'.join(result))
        return result

    def basic_test(self, inp, query, out, **k):
        result = self.run_matcher(inp, query, **k)
        if out is not None:
            if hasattr(out, 'splitlines'):
                out = out.splitlines()
            self.assertEqual(list(out), result)
        return out

    def test_filtering(self):
        ' Non matching entries must be removed '
        self.basic_test('test\nxyz', 'te', 'test')
        self.basic_test('abc\nxyz', 'ba', '')
        self.basic_test('abc\n123', 'abc', 'abc')

    def test_case_insesitive(self):
        self.basic_test('test\nxyz', 'Te', 'test')
        self.basic_test('test\nxyz', 'XY', 'xyz')
        self.basic_test('test\nXYZ', 'xy', 'XYZ')
        self.basic_test('test\nXYZ', 'mn', '')

    def test_marking(self):
        ' Marking of matched characters '
        self.basic_test(
            'test\nxyz',
            'ts',
            '\x1b[32mt\x1b[39me\x1b[32ms\x1b[39mt',
            mark=True)

    def test_threading(self):
        ' Test matching on a large data set with different number of threads '
        with open(os.path.join(base, 'test-data', 'qt-files.bz2'), 'rb') as f:
            data = bz2.decompress(f.read())
        for threads in range(4):
            self.basic_test(data, 'qt', None, threads=threads)


if __name__ == '__main__':
    unittest.main(verbosity=2)
