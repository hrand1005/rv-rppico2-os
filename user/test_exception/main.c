/**
 * @file main.c
 * @brief Creates illegal exceptions. Used for testing exception ISR dispatch.
 * @author Herbie Rand
 */

unsigned long load_align_fault();
unsigned long load_access_fault();

unsigned long store_align_fault();
unsigned long store_access_fault();

void illegal_instruction();

int main() {
    // Uncomment to trigger various fault conditions
    load_align_fault();
    // load_access_fault();
    // store_align_fault();
    // store_access_fault();
    // illegal_instruction();
    return 0;
}

/**
 * @brief Trigger a load alignment fault.
 */
unsigned long load_align_fault() {
    return *(unsigned long *)0x10000001;
}

/**
 * @brief Trigger a load access fault.
 */
unsigned long load_access_fault() {
    return *(unsigned long *)0xFFFFFFF0;
}

/**
 * @brief Trigger a store alignment fault.
 */
unsigned long store_align_fault() {
    *(unsigned long *)0x10000001 = 0xDEADBEEF;
    return 0;
}

/**
 * @brief Trigger a store access fault.
 */
unsigned long store_access_fault() {
    *(unsigned long *)0xFFFFFFF0 = 0xDEADBEEF;
    return 0;
}

/**
 * @brief Trigger an illegal instruction exception by executing invalid opcode.
 */
void illegal_instruction() {
    __asm__ volatile(".word 0x00000000");
}
