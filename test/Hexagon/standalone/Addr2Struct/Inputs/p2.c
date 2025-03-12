// Total padding: 4 Bytes

#define LEN 12

typedef unsigned char uint8;
typedef unsigned short uint16;

typedef enum { VAL1 = 0, VAL2, VAL3, VAL4 } ValueEnum;

typedef struct {
  ValueEnum val; // Padding 3 bytes after

  struct {
    char StrData[LEN];
    uint8 field_1;
    uint8 field_2;
    uint8 field_3; // Padding 1 byte after
    int field_4;
  } ValStruct;

} foo;

foo str; // padding 4 bytes

int main() { return str.ValStruct.field_4; }
