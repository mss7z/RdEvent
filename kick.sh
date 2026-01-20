#!/bin/bash

if [ -z $_CALL_BY_ME ]; then    
    _CALL_BY_ME=1
    export _CALL_BY_ME
    bash --rcfile $(cd $(dirname $0); pwd)/$(basename $0)
else
    source ~/.bashrc
    # source ./.venv/bin/activate
    unset _CALL_BY_ME
fi

