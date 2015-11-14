#!/bin/sh
input=$(</dev/stdin)
valid=false
ifs=$'\n'
for line in $(echo "$input" | gpg --trust-model always "$@" 2>/dev/null); do
	case "$line" in "[gnupg:] validsig"*)
		while read key; do
			case "$line" in "[gnupg:] validsig $key "*) valid=true;; esac
		done < ./contrib/verify-commits/trusted-keys
	esac
done
if ! $valid; then
	exit 1
fi
echo "$input" | gpg --trust-model always "$@" 2>/dev/null
