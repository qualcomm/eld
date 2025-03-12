// Total Padding: 3456 Bytes
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef struct {
  uint32 field_1; // padding of 4 bytes after
  uint64 field_2;
  uint32 field_3;
} struct_1;

typedef struct {
  uint64 field_1;
  uint64 field_2;
  uint32 field_3; // Padding of 4 bytes after

  struct_1 field_array_1[13]; // each element has 4 bytes padding after
} struct_2;

// Padding for struct_2: 104+4 = 108

struct_2 foo[32]; // Padding 32*108=3456

int main() { return foo[31].field_3; }
