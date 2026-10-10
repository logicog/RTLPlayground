#!/bin/bash
MACHINE="$1"
if [[ ! -s output-main/rtlplayground.map ]]; then
    original_branch=$(git branch --show-current)
    #trap 'git switch "$original_branch"' EXIT
    git switch main && make CI=1 MACHINE="$MACHINE" BUILDDIR=output-main
    git switch "$original_branch"
fi
source <(sed -rn 's/^([XCD]SEG|(BANK[0-9])).*=\s+([0-9]+)\. bytes.*$/main_\1=\3/p' output-main/rtlplayground.map | sort -u)
if [[ ! -s "output/$MACHINE/rtlplayground.map" ]]; then
    make CI=1 MACHINE="$MACHINE"
fi
source <(sed -rn 's/^([XCD]SEG|(BANK[0-9])).*=\s+([0-9]+)\. bytes.*$/branch_\1=\3/p' output/"$MACHINE"/rtlplayground.map | sort -u)

printf '%-20s %-20s %-20s %-20s\n' "segment" "main" "branch" "change"
printf '%-20s %-20s %-20s %-20s\n' "BANK1" "$main_BANK1" "$branch_BANK1" "$(($branch_BANK1-$main_BANK1))"
printf '%-20s %-20s %-20s %-20s\n' "BANK2" "$main_BANK2" "$branch_BANK2" "$(($branch_BANK2-$main_BANK2))"
printf '%-20s %-20s %-20s %-20s\n' "BANK3" "$main_BANK3" "$branch_BANK3" "$(($branch_BANK3-$main_BANK3))"
printf '%-20s %-20s %-20s %-20s\n' "CSEG" "$main_CSEG" "$branch_CSEG" "$(($branch_CSEG-$main_CSEG))"
printf '%-20s %-20s %-20s %-20s\n' "DSEG" "$main_DSEG" "$branch_DSEG" "$(($branch_DSEG-$main_DSEG))"
printf '%-20s %-20s %-20s %-20s\n' "XSEG" "$main_XSEG" "$branch_XSEG" "$(($branch_XSEG-$main_XSEG))"
