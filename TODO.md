# TODOs

- Moving .text or at least .vectors out of XIP Flash to SRAM, compare performance
- Alternately, pin vector table and associated handlers' cache lines
- `src/rp2_common/hardware_exception/exception_table_riscv.S` suggests special
ordering to saving caller saved registers for dealing with PMP exceptions, try
to understand what this is about.
- In exception / interrupt handling -- should we save floating point regs?
- When we initialize a multi-core runtime, we will probably want to enable machine software interrupts via `mie.msie`: `csrsi mie, 0x8u`
- When implementing kernel / user spaces, we will need user stack(s)
- During runtime initialization, if necessary, enable timer interrupts with `mie.mtie`
- profile and possibly implement last-chance check of IRQ before exiting `isr_mei`
- eliminate jump in mei interrupt handler?
- Compile all kernel modules independently, link in as required
- mtimer cache -- how much time did we save?
- Compile modules individually, make linker template instead of wildcard matching?
- Rewrite now invalid user mode applications, move some to tests
- Make PMP configuration helpers so that it isn't a huge pain in the ass and unreadable
- Read Debug Mode documentation; update DPC programatically to continue
