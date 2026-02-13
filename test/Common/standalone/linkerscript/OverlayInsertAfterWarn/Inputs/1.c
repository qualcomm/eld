__attribute__((section(".text.anchor"))) int anchor(void) { return 0; }
__attribute__((section(".text.ovl1"))) int overlay_func(void) { return 1; }
