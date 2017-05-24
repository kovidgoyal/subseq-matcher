#!/usr/bin/env python3
# vim:fileencoding=utf-8
# License: GPL v3 Copyright: 2017, Kovid Goyal <kovid at kovidgoyal.net>

from __future__ import print_function

import os
import re
import shlex
import subprocess

os.chdir(os.path.dirname(os.path.abspath(__file__)))
raw = open('cli.ggo', 'rb').read().decode('utf-8')
nv = re.search(
    r'^version\s+"(\d+).(\d+).(\d+)"',
    raw, flags=re.MULTILINE)
version = '%s.%s.%s' % (nv.group(1), nv.group(2), nv.group(3))
appname = re.search(
    r'^package\s+"([^"]+)"', raw, flags=re.MULTILINE).group(1)


def call(*cmd):
    if len(cmd) == 1:
        cmd = shlex.split(cmd[0])
    ret = subprocess.Popen(cmd).wait()
    if ret != 0:
        raise SystemExit(ret)


def run_tag():
    call('git tag -s v{0} -m version-{0}'.format(version))
    call('git push origin v{0}'.format(version))


def main():
    for action in ('tag',):
        print('Running', action)
        cwd = os.getcwd()
        globals()['run_' + action]()
        os.chdir(cwd)


if __name__ == '__main__':
    main()
