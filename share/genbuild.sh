#!/bin/sh
if [ $# -gt 1 ]; then
    cd "$2"
fi
if [ $# -gt 0 ]; then
    file="$1"
    shift
    if [ -f "$file" ]; then
        info="$(head -n 1 "$file")"
    fi
else
    echo "usage: $0 <filename> <srcroot>"
    exit 1
fi

desc=""
suffix=""
last_commit_date=""
if [ -e "$(which git 2>/dev/null)" -a "$(git rev-parse --is-inside-work-tree 2>/dev/null)" = "true" ]; then
    # clean 'dirty' status of touched files that haven't been modified
    git diff >/dev/null 2>/dev/null 

    # if latest commit is tagged and not dirty, then override using the tag name
    rawdesc=$(git describe --abbrev=0 2>/dev/null)
    if [ "$(git rev-parse head)" = "$(git rev-list -1 $rawdesc 2>/dev/null)" ]; then
        git diff-index --quiet head -- && desc=$rawdesc
    fi

    # otherwise generate suffix from git, i.e. string like "59887e8-dirty"
    suffix=$(git rev-parse --short head)
    git diff-index --quiet head -- || suffix="$suffix-dirty"

    # get a string like "2012-04-10 16:27:19 +0200"
    last_commit_date="$(git log -n 1 --format="%ci")"
fi

if [ -n "$desc" ]; then
    newinfo="#define build_desc \"$desc\""
elif [ -n "$suffix" ]; then
    newinfo="#define build_suffix $suffix"
else
    newinfo="// no build information available"
fi

# only update build.h if necessary
if [ "$info" != "$newinfo" ]; then
    echo "$newinfo" >"$file"
    if [ -n "$last_commit_date" ]; then
        echo "#define build_date \"$last_commit_date\"" >> "$file"
    fi
fi
