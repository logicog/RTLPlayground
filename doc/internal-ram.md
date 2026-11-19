# Internal RAM and the stack

The internal RAM (see [hardware.md](hardware.md)) holds the register banks,
the bit variables, `DSEG`, the overlay segment and the stack. All but the
stack have to sit below `0x80`, where direct addressing reaches. The stack
starts behind the last of them and grows up to `0xff`.

The linker writes the result to `output/<machine>/rtlplayground.mem`:

```
16 bit mode initial stack starts at: 0x71 (sp set to 0x70) with 143 bytes available.
```

`make bank_size` and the CI job summary print both sides of this next to
main: `direct RAM free` is what is left below `0x80`, `stack available` is
the room for the stack.

When the direct RAM is used up, the build fails at link time with
`Could not get N consecutive bytes in internal RAM`. The stack then still
has the 128 bytes above `0x80`.

Nothing reports a stack that grows past `0xff`: the stack pointer wraps
into the register banks. How deep the stack went on a running switch is
shown by `health`, see [health.md](health.md).

## Local variables

SDCC keeps an automatic variable in registers, also when it is declared
`__xdata`. A value that has to survive a call is saved in a spill slot
(`sloc`) in `DSEG`, so it costs direct RAM and with that stack.

- Use `static __xdata` for the locals of a function that is not re-entered.
  A static is not kept in registers.
- Use `volatile __xdata` where a static would have the wrong lifetime. It
  costs more code than the static.
- `--opt-code-size` and `--max-allocs-per-node` do not change where the
  spill slots go.

`grep sloc output/<machine>/<file>.asm` shows the slots of a file.
