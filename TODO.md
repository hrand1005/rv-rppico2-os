# TODOs

There are a lot of decisions that I've made without any practical justification.
Here are some things I'd like to try:

- Moving .text or at least .vectors out of XIP Flash to SRAM, compare performance
- Alternately, pin vector table and associated handlers' cache lines
- `src/rp2_common/hardware_exception/exception_table_riscv.S` suggests special
ordering to saving caller saved registers for dealing with PMP exceptions, try
to understand what this is about.
- In exception / interrupt handling -- should we save floating point regs?
- When we initialize a multi-core runtime, we will probably want to enable machine software interrupts via `mie.msie`: `csrsi mie, 0x8u`
- When implementing kernel / user spaces, we will need user stack(s)
- Start sticking registers in a `.h` file for use in `startup.S`.
- During runtime initialization, if necessary, enable timer interrupts with `mie.mtie`
