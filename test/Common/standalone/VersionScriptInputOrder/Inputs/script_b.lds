SECTIONS
{
  .text : { *(.text) }
}

VERSION
{
  VER_B {
  global:
    a*;
  local:
    *;
  };
}
