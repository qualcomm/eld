#ifdef PULLFROMARCHIVE
int main() { return foo() + __real_foo(); }
#endif
#ifdef NOPULLFROMARCHIVE
int main() { return foo(); }
#endif
