#!/bin/bash
MACHINE="$1"
MAIN="${2:-output-main}"
BRANCH="${3:-output/$MACHINE}"
if [[ ! -s "$MAIN/rtlplayground.map" ]]; then
    original_branch=$(git branch --show-current)
    #trap 'git switch "$original_branch"' EXIT
    git switch main && make CI=1 MACHINE="$MACHINE" BUILDDIR="$MAIN"
    git switch "$original_branch"
fi
if [[ ! -s "$BRANCH/rtlplayground.map" ]]; then
    make CI=1 MACHINE="$MACHINE" BUILDDIR="$BRANCH"
fi

seg() {
    sed -rn "s/^$1[^=]*=\s+([0-9]+)\. bytes.*$/\1/p" "$2/rtlplayground.map" | head -n 1
}

stack() {
    sed -rn 's/.* with ([0-9]+) bytes available.*/\1/p' "$1/rtlplayground.mem"
}

web() {
    du -sb "$1/html_min" | awk '{print $1}'
}

print_line() {
  local name=$1 main=$2 branch=$3
  printf '| %-15s | %7s | %7s | %7s |\n' "$name" "$main" "$branch" "$((${branch:-0} - ${main:-0}))"
}

echo "### $MACHINE"
echo
printf '| %-15s | %7s | %7s | %7s |\n' "segment" "main" "branch" "change"
echo "| --------------- | ------: | ------: | ------: |"
for s in HOME CSEG BANK1 BANK2 BANK3 BANK4 DSEG XSEG; do
    print_line "$s" "$(seg "$s" "$MAIN")" "$(seg "$s" "$BRANCH")"
done
print_line "stack available" "$(stack "$MAIN")" "$(stack "$BRANCH")"
print_line "WEB" "$(web "$MAIN")" "$(web "$BRANCH")"
