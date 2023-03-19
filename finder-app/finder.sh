#!/bin/sh

filesdir=$1
searchstr=$2

usage() {
    echo "usage: $0 <filesdir> <searchstr>"
}

if [ -z "$filesdir" ]; then
    usage
    exit 1
fi
if [ -z "$searchstr" ]; then
    usage
    exit 1
fi
if [ ! -d "$filesdir" ]; then
    usage
    exit 1
fi

total_files=`find "$filesdir" -type f | wc -l`
match_files=`grep -r "$searchstr" "$filesdir" | wc -l`

echo "The number of files are $total_files and the number of matching lines are $match_files"

