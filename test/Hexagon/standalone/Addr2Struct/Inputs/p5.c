// TOTAL padding: 420 Bytes
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char uint8;

typedef struct {
  uint32 field1; // 4 B padding after
  uint64 field2;
  uint64 field3;
  uint32 field4;
  uint8 field5;
} struct1;

#define LEN 60

static struct1 struct_array[LEN]; // 3 B alignment padding

int main() { return struct_array[10].field1; }
