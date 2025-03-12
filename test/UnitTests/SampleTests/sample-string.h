#include <string.h>


// A simple string class.
class MyString {
 private:
  const char* c_string_;
  const MyString& operator=(const MyString& rhs);

 public:
  // Clones a 0-terminated C string, allocating memory using new.
  static const char* CloneCString(const char* a_c_string);

  ////////////////////////////////////////////////////////////
  //
  // C'tors

  // The default c'tor constructs a NULL string.
  MyString() : c_string_(NULL) {}

  // Constructs a MyString by cloning a 0-terminated C string.
  explicit MyString(const char* a_c_string) : c_string_(NULL) {
    Set(a_c_string);
  }

  // Copy c'tor
  MyString(const MyString& string) : c_string_(NULL) {
    Set(string.c_string_);
  }

  ////////////////////////////////////////////////////////////
  //
  // D'tor.  MyString is intended to be a final class, so the d'tor
  // doesn't need to be virtual.
  ~MyString() { delete[] c_string_; }

  // Gets the 0-terminated C string this MyString object represents.
  const char* c_string() const { return c_string_; }

  size_t Length() const {
    return c_string_ == NULL ? 0 : strlen(c_string_);
  }

  // Sets the 0-terminated C string this MyString object represents.
  void Set(const char* c_string);
};

