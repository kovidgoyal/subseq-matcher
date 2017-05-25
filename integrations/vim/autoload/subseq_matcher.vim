" =======================================================================
" Version:     1.0.0
" Description: Vim plugin that add subseq matching to ctrl-p
" Maintainer:  Kovid Goyal <kovid at kovidgoyal.net>
" License:     Copyright (C) 2017 Kovid Goyal (Vim License)
" The same license that vim is distributed under. See :help license
" ======================================================================

fu! s:init()
    if exists("g:loaded_subseq_matcher")
        return
    endif

    if !has('python3')
        echoerr 'subseq-matcher requires vim compiled with python 3'
    endif

    if !exists('g:subseq_matcher_exe')
        let g:subseq_matcher_exe = has('win32') ? 'subseq-matcher.exe' : 'subseq-matcher'
        if !executable(g:subseq_matcher_exe)
            echoerr 'the '.g:subseq_matcher_exe.' executable was not found.'
            return
        endif
    endif

    let g:loaded_subseq_matcher = 1
python3 << ImportEOF
def ctrlp_subseq_implementation():
    import os
    import subprocess
    import sys
    import vim
    exe = vim.eval('g:subseq_matcher_exe')
    ispy3 = sys.version_info.major >= 3
    if hasattr(sys, 'getwindowsversion'):
        import msvcrt

        def popen(cmd):
            ans = subprocess.Popen(
                [exe] + cmd,
                stdout=subprocess.PIPE,
                stdin=subprocess.PIPE,
                creationflags=0x08)
            msvcrt.set_mode(ans.stdin.fileno(), os.O_BINARY)
            msvcrt.set_mode(ans.stdout.fileno(), os.O_BINARY)
            return ans
    else:

        def popen(cmd):
            return subprocess.Popen(
                [exe] + cmd, stdout=subprocess.PIPE, stdin=subprocess.PIPE)

    mmode_map = {
        'filename-only': os.path.basename,
        'first-non-tab': lambda x: x.split('\t', 1)[-1],
        'until-last-tab': lambda x: x.rpartition('\t')[0],
    }

    def process_results(results, line_map, needs_offset):
        rlen = len(results)
        for lnum, line in enumerate(results):
            positions, line = line.partition(':')[::2]
            orig_line = line_map.get(line, line)
            offset = 3  # offset in CtrlP window
            if orig_line is not line and needs_offset:
                offset += len(orig_line) - len(line)
            # matchaddpos() takes at most 8 positions
            positions = [offset + int(p) for p in positions.split(',')][:8]
            positions = ('[{},{}]'.format(rlen - lnum, pos) for pos in positions)
            vim.command('call matchaddpos("CtrlPMatch", [{}])'.format(','.join(positions)))
            yield orig_line

    def ctrlp(lines, query, limit, mmode, ispath):
        f = mmode_map.get(mmode)
        line_map = {} if f is None else {f(l): l for l in lines}
        inp = '\n'.join(lines).encode('utf-8')
        if not ispy3:
            query = query.encode('utf-8')
        cmd = ['-p', query]
        if limit > 0:
            cmd.extend(['--limit', str(limit)])
        p = popen(cmd)
        results = p.communicate(inp)[0].decode('utf-8').splitlines()
        results = list(
            process_results(results, line_map, mmode != 'until-last-tab'))
        return results

    return ctrlp

ctrlp_subseq_matcher = ctrlp_subseq_implementation()
del ctrlp_subseq_implementation
ImportEOF
endfu

fu! s:matchfname(item, pat)
  let parts = split(a:item, '[\/]\ze[^\/]\+$')
  return match(parts[-1], a:pat)
endf

function! subseq_matcher#ctrlp_match(lines, input, limit, mmode, ispath, crfile, regex)
    " Clear matches, that left from previous matches
    call s:init()
    call clearmatches()
    if a:ispath && !empty(a:crfile)
        call remove(array, index(array, a:crfile))
    endif

    if a:input == ''
        " Hack to clear s:savestr flag in SplitPattern, otherwise matching in
        " 'tag' mode will work only from 2nd char.
        call ctrlp#call('s:SplitPattern', '')
        return a:lines[0:a:limit]
    endif

    if a:regex
        let array = []
        let func = a:mmode == "filename-only" ? 's:matchfname' : 'match'
        for item in a:lines
            if call(func, [item, a:input]) >= 0
                cal add(array, item)
            endif
        endfor
        call sort(array, ctrlp#call('s:mixedsort'))
        let pat = ""
        if a:mmode == "filename-only"
            let pat = substitute(a:input, '\$\@<!$', '\\ze[^\\/]*$', 'g')
        endif
        if empty(pat)
            let pat = substitute(a:input, '\\\@<!\^', '^> \\zs', 'g')
        endif
        call matchadd('CtrlPMatch', '\c'.pat)
        return array[0:a:limit]
    endif

python3 << EOF
subseq_matches = ctrlp_subseq_matcher(vim.eval('a:lines'), vim.eval('a:input'), int(vim.eval('a:limit')), vim.eval('a:mmode'), bool(int(vim.eval('a:ispath'))))
EOF
    return py3eval('subseq_matches')
    endfunction
