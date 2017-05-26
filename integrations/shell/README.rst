Quickly execute commands from your history
============================================

First add the `choose <choose>`_ script to your ``PATH`` and make it executable (it
is a python script). It will provide a nice full screen terminal interface to
browse your history. 

zsh
------

Add the following snippet to your ``~/.zshrc`` to have ``Ctrl-R`` launch the
interactive history browser:

.. code-block::

    function select_history() {
        local choice
        choice=$(fc -l -n 1 | choose -- "$LBUFFER")
        if [[ -n "$choice" ]]; then
            LBUFFER=""
            zle -U $choice
        fi
    }

    zle -N select_history
    bindkey '^R' select_history


Integrations with other shells will have to wait for users of those shell to
contribute instructions.
