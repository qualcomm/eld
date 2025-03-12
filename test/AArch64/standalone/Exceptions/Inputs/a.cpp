struct X {
  X() {
   throw 1;
  }
};

int main() {
  try {
    X a;
  } catch (int i) {
  }
}

