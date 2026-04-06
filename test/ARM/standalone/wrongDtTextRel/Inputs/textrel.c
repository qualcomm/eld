int fn2(char *);
static char *blah = "quic";
int fn1() {
    return fn2(blah);
}
