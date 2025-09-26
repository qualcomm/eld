SECTIONS
{
  /* Exception handling frame */
  .eh_frame :
  {
    __eh_frame_start = .;
    KEEP(*(.eh_frame))
    __eh_frame_end = .;
  }
  /* Exception handling frame header */
  .eh_frame_hdr :
  {
    __eh_frame_hdr_start = .;
    KEEP(*(.eh_frame_hdr))
    __eh_frame_hdr_end = .;
  }
}
