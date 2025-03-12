int main () {
  try {
    throw 0x42;
  }
  catch (int e) {
    return 0;
  }
  return 1;
}

