#!/usr/bin/env python
# vim:fileencoding=utf-8
# License: GPLv3 Copyright: 2017, Kovid Goyal <kovid at kovidgoyal.net>

from __future__ import (absolute_import, division, print_function,
                        unicode_literals)

import argparse
import errno
import hashlib
import os
import re
import shlex
import subprocess
import sys
import sysconfig
from collections import namedtuple

self_path = os.path.abspath(__file__)
base = os.path.dirname(self_path)
build_dir = os.path.join(base, 'build')
_plat = sys.platform.lower()
isosx = 'darwin' in _plat
iswindows = hasattr(sys, 'getwindowsversion')
is_travis = os.environ.get('TRAVIS') == 'true'
Env = namedtuple('Env', 'cc cflags ldflags linker debug cc_name cc_ver')


def safe_makedirs(path):
    try:
        os.makedirs(path)
    except EnvironmentError as err:
        if err.errno != errno.EEXIST:
            raise


if iswindows:

    def cc_version():
        return 'cl.exe', (0, 0), 'cl'

    def get_sanitize_args(*a):
        return set()

    def init_env(debug=False, sanitize=False, native_optimizations=False):
        cc, ccver, cc_name = cc_version()
        cflags = '/c /nologo /MD /W3 /EHsc /DNDEBUG'.split()
        ldflags = []
        return Env(cc, cflags, ldflags, 'link.exe', debug, cc_name, ccver)
else:

    def cc_version():
        cc = os.environ.get('CC', 'gcc')
        raw = subprocess.check_output(
            [cc, '-dM', '-E', '-'], stdin=open(os.devnull, 'rb'))
        m = re.search(br'^#define __clang__ 1', raw, flags=re.M)
        cc_name = 'gcc' if m is None else 'clang'
        ver = int(
            re.search(br'#define __GNUC__ (\d+)', raw, flags=re.M)
            .group(1)), int(
                re.search(br'#define __GNUC_MINOR__ (\d+)', raw, flags=re.M)
                .group(1))
        return cc, ver, cc_name

    def get_sanitize_args(cc, ccver):
        sanitize_args = set()
        if cc == 'gcc' and ccver < (4, 8):
            return sanitize_args
        sanitize_args.add('-fno-omit-frame-pointer')
        sanitize_args.add('-fsanitize=address')
        if (cc == 'gcc' and ccver >= (5, 0)) or (cc == 'clang' and not isosx):
            # clang on oS X does not support -fsanitize=undefined
            sanitize_args.add('-fsanitize=undefined')
            # if cc == 'gcc' or (cc == 'clang' and ccver >= (4, 2)):
            #     sanitize_args.add('-fno-sanitize-recover=all')
        return sanitize_args

    def init_env(debug=False, sanitize=False, native_optimizations=False):
        native_optimizations = (native_optimizations and not sanitize and
                                not debug)
        cc, ccver, cc_name = cc_version()
        print('CC:', cc, ccver, cc_name)
        stack_protector = '-fstack-protector'
        if ccver >= (4, 9) and cc_name == 'gcc':
            stack_protector += '-strong'
        missing_braces = ''
        if ccver < (5, 2) and cc_name == 'gcc':
            missing_braces = '-Wno-missing-braces'
        optimize = '-ggdb' if debug or sanitize else '-O3'
        sanitize_args = get_sanitize_args(cc_name,
                                          ccver) if sanitize else set()
        cflags = os.environ.get(
            'OVERRIDE_CFLAGS',
            ('-Wextra -Wno-missing-field-initializers -Wall -std=c99'
             ' -pedantic-errors -Werror {} {} -D{}DEBUG -fwrapv {} {} -pipe {}'
             ).format(optimize, ' '.join(sanitize_args), (''
                                                          if debug else 'N'),
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
        return Env(cc, cflags, ldflags, cc, debug, cc_name, ccver)


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
            if (x == 'windows_compat.c' and not iswindows) or (
                    x == 'unix_compat.c' and iswindows):
                continue
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
    with open('cli.ggo', 'rb') as f1, open(self_path, 'rb') as f2:
        current_sig = hashlib.sha256(f1.read() + f2.read()).hexdigest()
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
    if iswindows:
        cmd = [env.cc] + env.cflags + ['/Tc' + src] + ['/Fo' + obj]
    else:
        extra = []
        # The code generated by gengetopt has unused variables according to
        # gcc, but not clang. Since I dont control the code and there is no way
        # short of compiling a test executable to detect if the compiler is
        # clang or gcc
        if src.endswith('/cli.c') and env.cc_name == 'gcc':
            extra.append('-Wno-unused-but-set-variable')
        cmd = [env.cc] + env.cflags + extra + ['-c', src] + ['-o', obj]
    run_tool(cmd)
    return obj


def build_exe(objects, env):
    suffix = '-debug' if env.debug else ''
    exe = os.path.join('build', 'subseq-matcher' + suffix)
    if iswindows:
        exe += '.exe'
        cmd = [env.linker] + objects + ['/OUT:' + exe] + env.ldflags
    else:
        cflags = list(env.cflags)
        if isosx:
            cflags.remove('-pthread')
        cmd = [env.linker] + cflags + objects + ['-o', exe] + env.ldflags
    run_tool(cmd)
    return exe


def build(args):
    getopt(args)
    sources, headers = find_c_files()
    if not iswindows:
        env = init_env(debug=True, sanitize=True)
        debug_objects = [build_obj(c, env) for c in sources]
        build_exe(debug_objects, env)
    env = init_env()
    objects = [build_obj(c, env) for c in sources]
    build_exe(objects, env)


def main():
    args = option_parser().parse_args()
    os.chdir(base)
    safe_makedirs(build_dir)
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
