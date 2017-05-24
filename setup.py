#!/usr/bin/env python
# vim:fileencoding=utf-8
# License: GPLv3 Copyright: 2017, Kovid Goyal <kovid at kovidgoyal.net>

from __future__ import (absolute_import, division, print_function,
                        unicode_literals)

import argparse
import errno
import hashlib
import os
import shlex
import subprocess
import sys
import sysconfig
from collections import namedtuple

base = os.path.dirname(os.path.abspath(__file__))
build_dir = os.path.join(base, 'build')
_plat = sys.platform.lower()
isosx = 'darwin' in _plat
is_travis = os.environ.get('TRAVIS') == 'true'
Env = namedtuple('Env', 'cc cflags ldflags debug')


def safe_makedirs(path):
    try:
        os.makedirs(path)
    except EnvironmentError as err:
        if err.errno != errno.EEXIST:
            raise


def cc_version():
    cc = os.environ.get('CC', 'gcc')
    raw = subprocess.check_output([cc, '-dumpversion']).decode('utf-8')
    ver = raw.split('.')[:2]
    try:
        ver = tuple(map(int, ver))
    except Exception:
        ver = (0, 0)
    return cc, ver


def get_sanitize_args(cc, ccver):
    sanitize_args = set()
    sanitize_args.add('-fno-omit-frame-pointer')
    sanitize_args.add('-fsanitize=address')
    if (cc == 'gcc' and ccver >= (5, 0)) or cc == 'clang':
        sanitize_args.add('-fsanitize=undefined')
        # if cc == 'gcc' or (cc == 'clang' and ccver >= (4, 2)):
        #     sanitize_args.add('-fno-sanitize-recover=all')
    return sanitize_args


def init_env(debug=False, sanitize=False, native_optimizations=False):
    native_optimizations = native_optimizations and not sanitize and not debug
    cc, ccver = cc_version()
    print('CC:', cc, ccver)
    stack_protector = '-fstack-protector'
    if ccver >= (4, 9) and cc == 'gcc':
        stack_protector += '-strong'
    missing_braces = ''
    if ccver < (5, 2) and cc == 'gcc':
        missing_braces = '-Wno-missing-braces'
    optimize = '-ggdb' if debug or sanitize else '-O3'
    sanitize_args = get_sanitize_args(cc, ccver) if sanitize else set()
    cflags = os.environ.get(
        'OVERRIDE_CFLAGS',
        ('-Wextra -Wno-missing-field-initializers -Wall -std=c99'
         ' -pedantic-errors -Werror {} {} -D{}DEBUG -fwrapv {} {} -pipe {}'
         ).format(optimize, ' '.join(sanitize_args), ('' if debug else 'N'),
                  stack_protector, missing_braces, '-march=native'
                  if native_optimizations else ''))
    cflags = shlex.split(cflags) + shlex.split(
        sysconfig.get_config_var('CCSHARED'))
    ldflags = os.environ.get('OVERRIDE_LDFLAGS',
                             '-Wall ' + ' '.join(sanitize_args) +
                             ('' if debug else ' -O3'))
    ldflags = shlex.split(ldflags)
    cflags += shlex.split(os.environ.get('CFLAGS', ''))
    ldflags += shlex.split(os.environ.get('LDFLAGS', ''))
    cflags.append('-pthread')
    safe_makedirs(build_dir)
    return Env(cc, cflags, ldflags, debug)


def define(x):
    return '-D' + x


def run_tool(cmd):
    if hasattr(cmd, 'lower'):
        cmd = shlex.split(cmd)
    print(' '.join(cmd))
    p = subprocess.Popen(cmd)
    ret = p.wait()
    if ret != 0:
        raise SystemExit(ret)


def newer(dest, *sources):
    try:
        dtime = os.path.getmtime(dest)
    except EnvironmentError:
        return True
    for s in sources:
        if os.path.getmtime(s) >= dtime:
            return True
    return False


def option_parser():
    p = argparse.ArgumentParser()
    p.add_argument(
        'action',
        nargs='?',
        default='build',
        choices='build test show-help getopt'.split(),
        help='Action to perform (default is build)')
    return p


def find_c_files():
    ans, headers = [], []
    src_dir = base
    for x in os.listdir(src_dir):
        ext = os.path.splitext(x)[1]
        if ext == '.c':
            ans.append(os.path.join(src_dir, x))
        elif ext == '.h':
            headers.append(os.path.join(src_dir, x))
    ans.sort(
        key=lambda x: os.path.getmtime(os.path.join(src_dir, x)), reverse=True)
    return tuple(ans), tuple(headers)


def getopt(args, show_help=False):
    if show_help:
        run_tool('gengetopt -i cli.ggo --show-help --default-optional')
        return
    try:
        with open('cli.c', 'rb') as f:
            line = f.read().decode('utf-8').splitlines()[0]
        sig = line[3:-3]
    except Exception:
        sig = None
    with open('cli.ggo', 'rb') as f:
        current_sig = hashlib.sha256(f.read()).hexdigest()
    if current_sig != sig:
        run_tool('gengetopt -i cli.ggo -F cli -u --default-optional -G')
        with open('cli.c', 'r+b') as f:
            raw = f.read().decode('utf-8')
            raw = '/* ' + current_sig + ' */\n' + raw
            f.seek(0)
            f.write(raw.encode('utf-8'))


def build_obj(src, env):
    suffix = '-debug' if env.debug else ''
    obj = os.path.join(
        'build', os.path.basename(src).rpartition('.')[0] + suffix + '.o')
    extra = []
    if src.endswith('cli.c'):
        # The code generated by gengetopt has unused variables
        extra.append('-Wno-unused-but-set-variable')
    cmd = [env.cc] + env.cflags + extra + ['-c', src] + ['-o', obj]
    run_tool(cmd)
    return obj


def build_exe(objects, env):
    suffix = '-debug' if env.debug else ''
    exe = os.path.join('build', 'subseq-matcher' + suffix)
    cmd = [env.cc] + objects + ['-o', exe] + env.ldflags
    run_tool(cmd)
    return exe


def build(args):
    getopt(args)
    sources, headers = find_c_files()
    env = init_env(debug=True, sanitize=True)
    debug_objects = [build_obj(c, env) for c in sources]
    build_exe(debug_objects, env)
    env = init_env()
    objects = [build_obj(c, env) for c in sources]
    build_exe(objects, env)


def main():
    args = option_parser().parse_args()
    os.chdir(base)
    if args.action == 'build':
        build(args)
    elif args.action == 'test':
        os.execlp(sys.executable, sys.executable, os.path.join(
            base, 'test.py'))
    elif args.action == 'getopt':
        getopt(args)
    elif args.action == 'show-help':
        getopt(args, show_help=True)


if __name__ == '__main__':
    main()
