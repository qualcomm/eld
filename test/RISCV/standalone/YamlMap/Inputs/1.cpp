/* Introduce a C++ COMDAT group */
class C {
public:
  static int k;
  C() { k = 100; }
  static int getVal() { return k; }
};

int C::k = 0;

C c;

int main() {
  try {
    throw c.getVal();
  } catch (int &i) {
    return i;
  }
  return 0;
}
