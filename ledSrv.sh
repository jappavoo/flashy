#!/bin/bash
set -x
INPIPE=~/.ledsin
if [[ ! -p $INPIPE ]]; then
  [[ -e $INPIPE ]] && rm $INPIPE
  if ! mkfifo $INPIPE; then
    echo ERROR could not mkfifo $INPIPE
    exit -1
  fi
fi

#tail -f $INPIPE |  python ble.py
tail -f $INPIPE | ./flashy

rm $INPIPE
