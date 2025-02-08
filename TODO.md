# TODOs

- Moving .text or at least .vectors out of XIP Flash to SRAM, compare performance
- Alternately, pin vector table and associated handlers' cache lines
- `src/rp2_common/hardware_exception/exception_table_riscv.S` suggests special
ordering to saving caller saved registers for dealing with PMP exceptions, try
to understand what this is about.
- In exception / interrupt handling -- should we save floating point regs?
- When we initialize a multi-core runtime, we will probably want to enable machine software interrupts via `mie.msie`: `csrsi mie, 0x8u`
- profile and possibly implement last-chance check of IRQ before exiting `isr_mei`
- eliminate jump in mei interrupt handler?
- mtimer cache -- how much time did we save?
- Rewrite now invalid user mode applications, move some to tests
- Make PMP configuration helpers so that it isn't a huge pain in the ass and unreadable
- Read Debug Mode documentation; update DPC programatically to continue
- Implement UART Console with minicom
- Fix inconsistent function naming --> e.g. some setters are thing_set, others are set_thing. Make them all like the former. Likewise with thing_init and init_thing
- Use the ATOMIC address offsets for setting/clearing/xoring bits. Audit the code and do this at some point.
- inline some of the shorter functions
- Rename test subidrs to test_<testname> -> <testname>
