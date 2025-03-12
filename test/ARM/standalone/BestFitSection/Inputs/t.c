void foo()
{
}

__attribute__ ((aligned (0x200)))
void (*const c1 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};

__attribute__ ((aligned (0x100)))
void (*const c2 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};

__attribute__ ((aligned (0x40)))
void (*const c3 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};


__attribute__ ((aligned (0x10)))
void (*const c4 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};

__attribute__ ((aligned (0x8)))
void (*const c5 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};

__attribute__ ((aligned (0x4)))
void (*const c6 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};


__attribute__ ((aligned (0x1)))
void (*const c7 [16]) () = {
    0,                                  // [0] SP_main - irrelevant in our case
    0,                             // [1] Reset

    // Cortex M3 interrupts

    0,     // [2] NMI
    0,     // [3] HardFault
    0,     // [4] MemManage
    0,     // [5] BusFault
    0,     // [6] UsageFault
    0,    // [7] reserved
    0,    // [8] reserved
    0,    // [9] reserved
    0,    // [10] reserved
    0,     // [11] SVCall
    0,     // [12] Debug Monitor
    0,    // [13] reserved
    0,     // [14] PendSV
    0,     // [15] SysTick
};


