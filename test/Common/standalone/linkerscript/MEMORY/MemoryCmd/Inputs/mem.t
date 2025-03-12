MEMORY
{
    bootloader (rx) : ORIGIN = 1000 + 10 * 100, LENGTH = 100*20
    writable (w!rx) : ORIGIN = 1000 + 10 * 100, LENGTH = 100*20
    INCLUDE foo.t
    INCLUDE_OPTIONAL nonexistent.t
}

