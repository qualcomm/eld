#include "Expected.h"
#include <memory>
#include <ostream>
#include <utility>
#include <iostream>
#include <string>
#define show(x) std::cout << #x << ": " << x << "\n";

class CWithoutMoveConstructor {
public:
  CWithoutMoveConstructor() = default;
  CWithoutMoveConstructor(CWithoutMoveConstructor &&) = default;
  CWithoutMoveConstructor(int pval) : val(pval) {}
  CWithoutMoveConstructor(const CWithoutMoveConstructor &) = default;
  CWithoutMoveConstructor &operator=(const CWithoutMoveConstructor &) = default;
  int val = 0;
};

class CWithoutDefaultConstructor {
public:
  CWithoutDefaultConstructor() = delete;
  CWithoutDefaultConstructor(const CWithoutDefaultConstructor&) = default;
  CWithoutDefaultConstructor(CWithoutDefaultConstructor&&) = default;
  CWithoutDefaultConstructor &operator=(const CWithoutDefaultConstructor&) = default;
  CWithoutDefaultConstructor &operator=(CWithoutDefaultConstructor&&) = default;
  CWithoutDefaultConstructor(double pi) : i(pi) {}
  double i;
};

std::ostream& operator<<(std::ostream& os, const CWithoutDefaultConstructor& other) {
  return (os << other.i);
}

int main() {
  eld::Expected<int, std::string> e1(11), e2("unexpected");
  show(e1.has_value());
  show(e1.value());
  show(e2.has_value());
  show(e2.error());

  eld::Expected<int, std::string> e3 = e1;
  eld::Expected<int, std::string> e4 = eld::Expected<int, std::string>(19);
  show(e3.has_value());
  show(e3.value());
  show(e4.has_value());
  show(e4.value());
  show((e1 == e3));
  show((e1 == e4));
  show((e1 != e3));
  show((e1 != e4));

  std::string unexpected = "unexpected";
  eld::Expected<int, std::string> e5(unexpected);
  std::cout << e5.has_value() << " " << e5.error() << "\n";
  std::cout << (e2 == e5) << " " << (e2 != e5) << "\n";

  e3 = e2;
  if (!e3)
    std::cout << e3.has_value() << " " << e3.error() << "\n";

  e2 = std::move(e1);
  if (e2)
    std::cout << e2.has_value() << " " << e2.value() << "\n";

  eld::Expected<std::unique_ptr<int>, std::unique_ptr<std::string>> e6{std::make_unique<int>(19)};
  eld::Expected<std::unique_ptr<int>, std::unique_ptr<std::string>> e7{std::make_unique<std::string>("move-only error")};
  show(e6.has_value());
  show(*e6.value());
  show(e7.has_value());
  show(*e7.error());

  std::unique_ptr<int> up_i = std::move(e6.value());
  show(*up_i);

  e6 = std::move(e7);
  show(e6.has_value());
  show(*e6.error());


  eld::Expected<CWithoutMoveConstructor, std::string> e8(CWithoutMoveConstructor(23));
  show(e8.has_value());
  show(e8.value().val);
  eld::Expected<CWithoutMoveConstructor, std::string> e9("copy-only error");
  show(e9.has_value());
  show(e9.error());
  eld::Expected<CWithoutMoveConstructor, std::string> e10;
  show(e10.has_value());
  show(e10.value().val);
  e8 = std::move(e9);
  show(e8.has_value())
  show(e8.error());
  e10 = e8;
  show(e10.has_value());
  show(e10.error());

  eld::Expected<CWithoutDefaultConstructor, std::string> e11(27), e12("error");
  show(e11.value());
  show(e12.error());
  e11 = e12;
  show(e11.error());

  eld::Expected<void, std::string> e13, e14("Error Message");
  eld::Expected<void, std::string> e15 = e14;

  show(e13.has_value());
  show(e14.error());
  show(e15.has_value());
  show(e15.error());
}