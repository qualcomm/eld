// Calling an external function forces a PLT/GOTPLT entry on ARM
// which triggers reportErrorIfGOTPLTIsDiscarded when .got.plt is discarded
extern int external_func();
int foo() { return external_func(); }
int bar() { return foo(); }
