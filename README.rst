subseq-matcher
######################

|unix_build| |windows_build|

A simple filter that reads lines from `STDIN`, ranks them using
subsequence-matching with a specified query and outputs the sorted list to
`STDOUT`. Designed to serve as the *engine* powering any keyboard based selection
interface. For example, quickly selecting a file in an editor, or quickly
executing a command from your shell history, with only a few keystrokes.

Look at the `integrations <integrations>`_ directory to see how to integrate
`subseq-matcher` with various programs.

.. |unix_build| image:: https://api.travis-ci.org/kovidgoyal/subseq-matcher.svg
    :target: http://travis-ci.org/kovidgoyal/subseq-matcher
    :alt: Build status of the master branch on Unix

.. |windows_build|  image:: https://ci.appveyor.com/api/projects/status/github/kovidgoyal/subseq-matcher?svg=true
    :target: https://ci.appveyor.com/project/kovidgoyal/subseq-matcher
    :alt: Build status of the master branch on Windows

