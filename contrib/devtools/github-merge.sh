#!/bin/bash

# this script will locally construct a merge commit for a pull request on a
# github repository, inspect it, sign it and optionally push it.

# the following temporary branches are created/overwritten and deleted:
# * pull/$pull/base (the current master we're merging onto)
# * pull/$pull/head (the current state of the remote pull request)
# * pull/$pull/merge (github's merge)
# * pull/$pull/local-merge (our merge)

# in case of a clean merge that is accepted by the user, the local branch with
# name $branch is overwritten with the merged result, and optionally pushed.

repo="$(git config --get githubmerge.repository)"
if [[ "d$repo" == "d" ]]; then
  echo "error: no repository configured. use this command to set:" >&2
  echo "git config githubmerge.repository <owner>/<repo>" >&2
  echo "in addition, you can set the following variables:" >&2
  echo "- githubmerge.host (default git@github.com)" >&2
  echo "- githubmerge.branch (default master)" >&2
  echo "- githubmerge.testcmd (default none)" >&2
  exit 1
fi

host="$(git config --get githubmerge.host)"
if [[ "d$host" == "d" ]]; then
  host="git@github.com"
fi

branch="$(git config --get githubmerge.branch)"
if [[ "d$branch" == "d" ]]; then
  branch="master"
fi

testcmd="$(git config --get githubmerge.testcmd)"

pull="$1"

if [[ "d$pull" == "d" ]]; then
  echo "usage: $0 pullnumber [branch]" >&2
  exit 2
fi

if [[ "d$2" != "d" ]]; then
  branch="$2"
fi

# initialize source branches.
git checkout -q "$branch"
if git fetch -q "$host":"$repo" "+refs/pull/$pull/*:refs/heads/pull/$pull/*"; then
  if ! git log -q -1 "refs/heads/pull/$pull/head" >/dev/null 2>&1; then
    echo "error: cannot find head of pull request #$pull on $host:$repo." >&2
    exit 3
  fi
  if ! git log -q -1 "refs/heads/pull/$pull/merge" >/dev/null 2>&1; then
    echo "error: cannot find merge of pull request #$pull on $host:$repo." >&2
    exit 3
  fi
else
  echo "error: cannot find pull request #$pull on $host:$repo." >&2
  exit 3
fi
if git fetch -q "$host":"$repo" +refs/heads/"$branch":refs/heads/pull/"$pull"/base; then
  true
else
  echo "error: cannot find branch $branch on $host:$repo." >&2
  exit 3
fi
git checkout -q pull/"$pull"/base
git branch -q -d pull/"$pull"/local-merge 2>/dev/null
git checkout -q -b pull/"$pull"/local-merge
tmpdir="$(mktemp -d -t ghmxxxxx)"

function cleanup() {
  git checkout -q "$branch"
  git branch -q -d pull/"$pull"/head 2>/dev/null
  git branch -q -d pull/"$pull"/base 2>/dev/null
  git branch -q -d pull/"$pull"/merge 2>/dev/null
  git branch -q -d pull/"$pull"/local-merge 2>/dev/null
  rm -rf "$tmpdir"
}

# create unsigned merge commit.
(
  echo "merge pull request #$pull"
  echo ""
  git log --no-merges --topo-order --pretty='format:%h %s (%an)' pull/"$pull"/base..pull/"$pull"/head
)>"$tmpdir/message"
if git merge -q --commit --no-edit --no-ff -m "$(<"$tmpdir/message")" pull/"$pull"/head; then
  if [ "d$(git log --pretty='format:%s' -n 1)" != "dmerge pull request #$pull" ]; then
    echo "error: creating merge failed (already merged?)." >&2
    cleanup
    exit 4
  fi
else
  echo "error: cannot be merged cleanly." >&2
  git merge --abort
  cleanup
  exit 4
fi

# run test command if configured.
if [[ "d$testcmd" != "d" ]]; then
  # go up to the repository's root.
  while [ ! -d .git ]; do cd ..; done
  if ! $testcmd; then
    echo "error: running $testcmd failed." >&2
    cleanup
    exit 5
  fi
  # show the created merge.
  git diff pull/"$pull"/merge..pull/"$pull"/local-merge >"$tmpdir"/diff
  git diff pull/"$pull"/base..pull/"$pull"/local-merge
  if [[ "$(<"$tmpdir"/diff)" != "" ]]; then
    echo "warning: merge differs from github!" >&2
    read -p "type 'ignore' to continue. " -r >&2
    if [[ "d$reply" =~ ^d[ii][gg][nn][oo][rr][ee]$ ]]; then
      echo "difference with github ignored." >&2
    else
      cleanup
      exit 6
    fi
  fi
  read -p "press 'd' to accept the diff. " -n 1 -r >&2
  echo
  if [[ "d$reply" =~ ^d[dd]$ ]]; then
    echo "diff accepted." >&2
  else
    echo "error: diff rejected." >&2
    cleanup
    exit 6
  fi
else
  # verify the result.
  echo "dropping you on a shell so you can try building/testing the merged source." >&2
  echo "run 'git diff head~' to show the changes being merged." >&2
  echo "type 'exit' when done." >&2
  if [[ -f /etc/debian_version ]]; then # show pull number in prompt on debian default prompt
      export debian_chroot="$pull"
  fi
  bash -i
  read -p "press 'm' to accept the merge. " -n 1 -r >&2
  echo
  if [[ "d$reply" =~ ^d[mm]$ ]]; then
    echo "merge accepted." >&2
  else
    echo "error: merge rejected." >&2
    cleanup
    exit 7
  fi
fi

# sign the merge commit.
read -p "press 's' to sign off on the merge. " -n 1 -r >&2
echo
if [[ "d$reply" =~ ^d[ss]$ ]]; then
  if [[ "$(git config --get user.signingkey)" == "" ]]; then
    echo "error: no gpg signing key set, not signing. set one using:" >&2
    echo "git config --global user.signingkey <key>" >&2
    cleanup
    exit 1
  else
    git commit -q --gpg-sign --amend --no-edit
  fi
else
  echo "not signing off on merge, exiting."
  cleanup
  exit 1
fi

# clean up temporary branches, and put the result in $branch.
git checkout -q "$branch"
git reset -q --hard pull/"$pull"/local-merge
cleanup

# push the result.
read -p "type 'push' to push the result to $host:$repo, branch $branch. " -r >&2
if [[ "d$reply" =~ ^d[pp][uu][ss][hh]$ ]]; then
  git push "$host":"$repo" refs/heads/"$branch"
fi
