/**
 * @file irq_test.c
 * @brief Test suite for machine external interrupt handling and preemption
 *
 * The test works by overriding weak ISR definitions and verifying the execution
 * order is recorded correctly in a global array based on priority levels.
 *
 * @author Herbie Rand
 */
void test_decreasing_prio();
void test_increasing_prio();

void breakpoint();
void isr_irq0();
void isr_irq1();
void isr_irq2();
void isr_irq4();
void isr_irq5();
void isr_irq6();

void pend_irq0();
void pend_irq1();
void pend_irq2();
void pend_irq4();
void pend_irq5();
void pend_irq6();

typedef unsigned long uint32_t;

static uint32_t exit[6];
static uint32_t idx = 0;

static uint32_t irq0 = 0x10000;
static uint32_t irq1 = 0x20000;
static uint32_t irq2 = 0x40000;

// static uint32_t irq3 = 0x80000;

static uint32_t irq4 = 0x100000;
static uint32_t irq5 = 0x200000;
static uint32_t irq6 = 0x400000;

// irq0 -> prio 3
// irq1 -> prio 2
// irq2 -> prio 1
static uint32_t prio1 = 0x0123 << 16;

// irq4 -> prio 3
// irq5 -> prio 2
// irq6 -> prio 1
static uint32_t prio2 = (0x0123 << 16) | 0x1;

int main() {
    test_decreasing_prio();
    test_increasing_prio();

    return 0;
}

/**
 * @brief Tests interruption prevention with decreasing priorities
 *
 * Configures IRQs 0-2 with decreasing priorities and verifies that
 * lower priority interrupts don't preempt higher priority ISRs.
 * Expected exit array after test: {0, 1, 2}
 */
void test_decreasing_prio() {
    // set relative priorities (irq0 > irq1 > irq2)
    asm volatile("csrw 0xbe3, %0" : : "r"(prio1));
    // enable mei irqs 0-2
    asm volatile("csrw 0xbe0, %0" : : "r"(irq0 | irq1 | irq2));
    pend_irq0();
    asm volatile("fence.i");
    breakpoint();
}

/**
 * @brief Tests interrupt preemption with increasing priorities
 *
 * Configures IRQs 4-6 with increasing priorities and verifies that
 * higher priority interrupts preempt lower priority ISRs.
 * Expected exit array after test: {0, 1, 2, 3, 4, 5}
 */
void test_increasing_prio() {
    // set relative priorities (irq3 > irq4 > irq5)
    asm volatile("csrw 0xbe3, %0" : : "r"(prio2));
    // enable mei irqs 4-6
    asm volatile("csrw 0xbe0, %0" : : "r"(irq4 | irq5 | irq6));
    // pend irq6, which will be interrupted by 5, then 4
    pend_irq6();
    asm volatile("fence.i");

    // at this breakpoint, exit should equal {0, 1, 2, 3, 4, 5}
    breakpoint();
}

void breakpoint() {
    asm volatile("ebreak");
}

void isr_irq0() {
    pend_irq1();
    exit[idx++] = 0;
}

void isr_irq1() {
    pend_irq2();
    exit[idx++] = 1;
}

void isr_irq2() {
    exit[idx++] = 2;
}

void isr_irq4() {
    exit[idx++] = 3;
}

void isr_irq5() {
    pend_irq4();
    exit[idx++] = 4;
}

void isr_irq6() {
    pend_irq5();
    exit[idx++] = 5;
}

void pend_irq0() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq0));
}

void pend_irq1() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq1));
}

void pend_irq2() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq2));
}

void pend_irq4() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq4));
}

void pend_irq5() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq5));
}

void pend_irq6() {
    asm volatile("csrw 0xbe2, %0" : : "r"(irq6));
}
