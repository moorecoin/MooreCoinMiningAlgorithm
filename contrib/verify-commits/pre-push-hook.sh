#!/bin/bash
if ! [[ "$2" =~ [git@]?[www.]?github.com[:|/]bitcoin/bitcoin[.git]? ]]; then
    exit 0
fi

while read line; do
    set -- a $line
    if [ "$4" != "refs/heads/master" ]; then
        continue
    fi
    if ! ./contrib/verify-commits/verify-commits.sh $3 > /dev/null 2>&1; then
        echo "error: a commit is not signed, can't push"
        ./contrib/verify-commits/verify-commits.sh
        exit 1
    fi
done < /dev/stdin
