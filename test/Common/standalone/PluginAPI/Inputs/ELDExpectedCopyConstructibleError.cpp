#include "Expected.h"
#include <memory>

int main() {
  eld::Expected<int, std::unique_ptr<int>> exp1;
}