#!/bin/bash 
INPIPE=~/.ledsin
MAXSLEEP=10

if [[ ! -a $INPIPE ]]; then
   echo "ERROR: $INPIPE does not exist is ledSrv running???"
   exit -1
fi

echo "B 1" > $INPIPE

while sleep 2; do 
#  echo "update" 
  echo $(echo "I"; 
         for ((i=0; i<96; i++)); do 
            echo $(($RANDOM % 255)); 
         done) > $INPIPE;
done
