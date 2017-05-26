Quick selection of files, buffers, etc.
===================================================================

You can easily integrate subseq-matcher into the excellent `ctrlp
<https://github.com/kien/ctrlp.vim>`_ vim plugin. To do so, just add the
`subseq_matcher.vim <autoload/subseq_matcher.vim>`_ file into
``~/.vim/autoload`` or similar and add the following to your
``~/.vimrc``:

.. code-block:: vim

    let g:ctrlp_match_func = {'match': 'subseq_matcher#ctrlp_match'}

