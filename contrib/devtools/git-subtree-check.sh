#!/bin/sh

dir="$1"
commit="$2"
if [ -z "$commit" ]; then
    commit=head
fi

# taken from git-subtree (copyright (c) 2009 avery pennarun <apenwarr@gmail.com>)
find_latest_squash()
{
	dir="$1"
	sq=
	main=
	sub=
	git log --grep="^git-subtree-dir: $dir/*\$" \
		--pretty=format:'start %h%n%s%n%n%b%nend%n' "$commit" |
	while read a b junk; do
		case "$a" in
			start) sq="$b" ;;
			git-subtree-mainline:) main="$b" ;;
			git-subtree-split:) sub="$b" ;;
			end)
				if [ -n "$sub" ]; then
					if [ -n "$main" ]; then
						# a rejoin commit?
						# pretend its sub was a squash.
						sq="$sub"
					fi
					echo "$sq" "$sub"
					break
				fi
				sq=
				main=
				sub=
				;;
		esac
	done
}

latest_squash="$(find_latest_squash "$dir")"
if [ -z "$latest_squash" ]; then
    echo "error: $dir is not a subtree" >&2
    exit 2
fi

set $latest_squash
old=$1
rev=$2
if [ "d$(git cat-file -t $rev 2>/dev/null)" != dcommit ]; then
    echo "error: subtree commit $rev unavailable. fetch/update the subtree repository" >&2
    exit 2
fi
tree_subtree=$(git show -s --format="%t" $rev)
echo "$dir in $commit was last updated to upstream commit $rev (tree $tree_subtree)"
tree_actual=$(git ls-tree -d "$commit" "$dir" | head -n 1)
if [ -z "$tree_actual" ]; then
    echo "fail: subtree directory $dir not found in $commit" >&2
    exit 1
fi
set $tree_actual
tree_actual_type=$2
tree_actual_tree=$3
echo "$dir in $commit currently refers to $tree_actual_type $tree_actual_tree"
if [ "d$tree_actual_type" != "dtree" ]; then
    echo "fail: subtree directory $dir is not a tree in $commit" >&2
    exit 1
fi
if [ "$tree_actual_tree" != "$tree_subtree" ]; then
    git diff-tree $tree_actual_tree $tree_subtree >&2
    echo "fail: subtree directory tree doesn't match subtree commit tree" >&2
    exit 1
fi
echo "good"
