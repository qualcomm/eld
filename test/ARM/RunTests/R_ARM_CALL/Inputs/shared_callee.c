// Preemptible symbol in a shared library: R_ARM_CALL goes through PLT.

__attribute__((noinline)) int shared_callee() { return 3; }