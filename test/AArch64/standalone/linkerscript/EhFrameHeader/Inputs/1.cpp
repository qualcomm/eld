extern "C" {
extern int bar();
}

int main() {
  try {
    return bar();
  } catch (...) {
    return 0;
  }
}
