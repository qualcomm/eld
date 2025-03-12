// TOTAL PADDING 4096 (4 * 1024)

#define BITS 10
#define LEN (1uL << BITS)
#define LEN_2 3

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef union {

  uint64 field_array_1[LEN_2];
  struct {
    uint32 field_1;
    uint32 field_2;
    uint32 field_3;
    uint32 field_4;
    uint32 field_5;
    uint32 field_6;
  };
} union_1;

typedef struct {
  uint32 field_1; // 4 B padding after
  union_1 field_2;
} struct_1;

volatile struct_1 test[LEN]; // LEN: 1024

int main() { return test[10].field_1; }
