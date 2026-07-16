/* Three preemptible functions */
void foo_both(void) {}      /* will be both called and address-taken */
void foo_call_only(void) {} /* will be called only (PLT32 only) */
void foo_addr_only(void) {} /* will be address-taken only (GOTPCREL only) */
