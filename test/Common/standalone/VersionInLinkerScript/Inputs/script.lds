SECTIONS
{
  .text : { *(.text) }
}

VERSION
{
  V1 {
  global:
    exported_symbol;
  local:
    *;
  };
}