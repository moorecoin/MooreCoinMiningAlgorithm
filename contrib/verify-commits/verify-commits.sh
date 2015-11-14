#!/bin/sh

dir=$(dirname "$0")

echo "please verify all commits in the following list are not evil:"
git log "$dir"

verified_root=$(cat "${dir}/trusted-git-root")

have_failed=false
is_signed () {
	if [ $1 = $verified_root ]; then
		return 0;
	fi
	if ! git -c "gpg.program=${dir}/gpg.sh" verify-commit $1 > /dev/null 2>&1; then
		return 1;
	fi
	local parents=$(git show -s --format=format:%p $1)
	for parent in $parents; do
		if is_signed $parent > /dev/null; then
			return 0;
		fi
	done
	if ! "$have_failed"; then
		echo "no parent of $1 was signed with a trusted key!" > /dev/stderr
		echo "parents are:" > /dev/stderr
		for parent in $parents; do
			git show -s $parent > /dev/stderr
		done
		have_failed=true
	fi
	return 1;
}

if [ x"$1" = "x" ]; then
	test_commit="head"
else
	test_commit="$1"
fi

is_signed "$test_commit"
res=$?
if [ "$res" = 1 ]; then
	if ! "$have_failed"; then
		echo "$test_commit was not signed with a trusted key!"
	fi
else
	echo "there is a valid path from $test_commit to $verified_root where all commits are signed!"
fi

exit $res
