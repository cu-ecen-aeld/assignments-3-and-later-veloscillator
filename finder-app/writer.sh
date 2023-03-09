#!/bin/bash

writefile=$1
writestr=$2

usage() {
    echo "usage: $0 <writefile> <writestr>"
}

if [ -z "$writefile" ]; then
    usage
    exit 1
fi
if [ -z "$writestr" ]; then
    usage
    exit 1
fi

d=`dirname "$writefile"`
if [ ! -z "$d" ]; then
    if [ ! -d "$d" ]; then
        mkdir -p "$d"
    fi
fi

echo "$writestr" > "$writefile"

