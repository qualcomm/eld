struct Guard {
  ~Guard() {}
};

void thrower() {
  Guard G;
  throw 42;
}
