#!/bin/bash
MACHINE="$1"
if [[ ! -s output-main/rtlplayground.map ]]; then
    original_branch=$(git branch --show-current)
    #trap 'git switch "$original_branch"' EXIT
    git switch main && make CI=1 MACHINE="$MACHINE" BUILDDIR=output-main
    git switch "$original_branch"
fi
source <(sed -rn 's/^([XCD]SEG|(BANK[0-9])).*=\s+([0-9]+)\. bytes.*$/main_\1=\3/p' output-main/rtlplayground.map | sort -u)

main_WEB=$(du -sb output-main/html_min | awk '{print $1}')
if [[ ! -s "output/$MACHINE/rtlplayground.map" ]]; then
    make CI=1 MACHINE="$MACHINE"
fi
source <(sed -rn 's/^([XCD]SEG|(BANK[0-9])).*=\s+([0-9]+)\. bytes.*$/branch_\1=\3/p' output/"$MACHINE"/rtlplayground.map | sort -u)
branch_WEB=$(du -sb output/html_min | awk '{print $1}')

print_line() {
  local name=$1 main=$2 branch=$3
  printf '%-20s %-20s %-20s %-20s\n' "$name" "$main" "$branch" "$((branch - main))"
}

printf '%-20s %-20s %-20s %-20s\n' "segment" "main" "branch" "change"

print_line "BANK1" "$main_BANK1" "$branch_BANK1"
print_line "BANK2" "$main_BANK2" "$branch_BANK2"
print_line "BANK3" "$main_BANK3" "$branch_BANK3"
print_line "CSEG" "$main_CSEG" "$branch_CSEG"
print_line "DSEG" "$main_DSEG" "$branch_DSEG"
print_line "XSEG" "$main_XSEG" "$branch_XSEG"
print_line "WEB" "$main_WEB" "$branch_WEB"
