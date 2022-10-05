#!/bin/bash

if [ $# -eq 0 ]
then
        cat<<EOM
   Usage: mktestinput.sh  FILENAME FILETYPE [ FILENAME FILETYPE [ ... ] ]
EOM
        exit
fi

BOUNDARY=-----------------------------7034497701176429139758495468

#
#  Wrap each binary file with boundary and HTTP headers and write to stdout
#
while [ $# -gt 0 ]
do
        FNAME=$1
        shift
        FTYPE=$1
        shift
cat<<EOM
$BOUNDARY
Content-Disposition: form-data; name="upload"; filename="$FNAME"
Content-Type: $FTYPE

EOM
        cat $FNAME
done


#
#  Write final boundary line
#

cat<<EOM

$BOUNDARY--
EOM
