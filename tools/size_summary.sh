#!/bin/sh
seg() {
	sed -rn "s/^$1[^=]*=\\s+([0-9]+)\\. bytes.*$/\\1/p" "$2/rtlplayground.map" | head -n 1
}
stack() {
	sed -rn 's/.* with ([0-9]+) bytes available.*/\1/p' "$1/rtlplayground.mem"
}
row() {
	echo "| $1 | $2 | $3 | $((${3:-0} - ${2:-0})) |"
}
echo "### KP_9000_6XHML_X2"
echo
echo "| | main | branch | change |"
echo "| --- | ---: | ---: | ---: |"
for s in HOME CSEG BANK1 BANK2 BANK3 BANK4 XSEG; do
	row "$s" "$(seg $s "$1")" "$(seg $s "$2")"
done
row "stack available" "$(stack "$1")" "$(stack "$2")"
row "web" "$(du -sb "$3" | cut -f1)" "$(du -sb "$4" | cut -f1)"
