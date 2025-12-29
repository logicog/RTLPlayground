# Understanding the image using ghidra
Start ghidra, load file starting from offset 0x0002 into
memory starting at 0x0000. The lengthe is 0x10000. Select generic 8051, big
endian.

After loading, the boot vector is at 0x0000, which will jump to 0x0100 for
the boot routine.

The firmware uses only bank 1 of the RTL837x since it is quite short.
Otherwise the firmware would be organized as follows
```
--------------------------- 0x0000 ---------------------------------
Boot-Vector
ISRs
Common Code
Trampoline for inter-bank calls
Inter-bank calls, calling trampoline, one for each callable function

----- Bank 1 0x4000 ------   ---- Bank 2 0x4000 -----  -------- .....
Overlay 1                    Overlay 2                 Overlay n

--------- 0xffff ---------   -------- 0xffff --------  -------- 0xffff
```
The RTL837x firmware images are organized as follows:
The first 2 bytes of the image give the size of the prefetched data at the
start of the CPU power up. The default is 0x4000 (bytes: 0x00 0x40), which
means that the entire shared area of the code memory in all banks,
0x4000 bytes is read immediately into the code RAM.

Common code starts at
0x0002 in the image and has length 0x3ffd, the first bank starts at 0x4000
in the image, is mapped to 0x4000 and has length 0xc000. The second bank
starts at 0x10000, is mapped to 0x4000 and has length 0xc000. The third
bank would start at 0x1c000 and would again be mapped to 0x4000.
There are about 30 banks in use for managed switches, unmanaged ones use
2-3, while the hardware would allow to use 0x3f banks, i.e. up to 4 MB of
flash.

The current image uses Common BANK0 and the first BANK1 via sdccs __banked
function keyword and custom banking trampoline code for the RTL837x in
assembler.
