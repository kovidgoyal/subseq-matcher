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

Installation
-------------

As simple as:

.. code-block:: sh

    make && sudo make install

This will install ``/usr/bin/subseq-matcher``. You can also run it without
installation directly from ``build/subseq-matcher``. 


Understanding the matching algorithm
----------------------------------------

The most basic fact about the matching algorithm is that it is a *subsequence
match*. This means that ``ct`` will match both ``act`` and ``cot``. It will 
match any string that contains the letters ``c`` followed somewhere by ``t``.

What makes it useful is the concept of *anchors*. These are special locations,
which increase the score of the ranking algorithm if a character matches at
them. For example, when filtering paths, it is useful to use the ``/``
character as an anchor. So the query ``abc`` will match both the paths:

 - /A/Better/Catch
 - /some/AlaBaster/torC

But, the first match will score higher. This matches the intuitive mental model we
use to find files inside a directory hierarchy. The mental route is something
like: *folder a* to *sub folder b* to *file c*. The query is naturally encoded
using the anchored-subsequence algorithm as simply ``abc``. This results in
fast, intuitive, minimum mental overhead navigation of directory hierarchies.

``subseq-matcher`` has three configurable levels of anchors. The defaults are
setup for general matching, prioritizing characters that occur after path and
extension separators (``/ and .``) as well as underscores, camelCase
characters and spaces. These can be easily changed via command line options
allowing the algorithm to be tuned for specific scenarios, as desired.

Run ``subseq-matcher -h`` for a list of command line options.


Performance
-------------

``subseq-matcher`` is written in C and uses a non-recursive, optimized
algorithm.  On my machine it can filter ``10,000`` strings with a query of four
characters in ``0.054s (avg)``, making it imperceptible to human senses.


Acknowledgements
------------------

Sub-sequence matching is obviously not new. It has a long history, the first
time I came across it personally, was in the ``Command-T`` and
``YouCompleteMe`` plugins for vim. However, as found myself wanting to use it
in more and more contexts, it made sense to separate out the core algorithm
into a standalone utility that makes it easy to integrate into different
contexts.


.. |unix_build| image:: https://api.travis-ci.org/kovidgoyal/subseq-matcher.svg
    :target: http://travis-ci.org/kovidgoyal/subseq-matcher
    :alt: Build status of the master branch on Unix

.. |windows_build|  image:: https://ci.appveyor.com/api/projects/status/github/kovidgoyal/subseq-matcher?svg=true
    :target: https://ci.appveyor.com/project/kovidgoyal/subseq-matcher
    :alt: Build status of the master branch on Windows

