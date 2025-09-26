extern char __eh_frame_start;
extern char __eh_frame_end;
extern char __eh_frame_hdr_start;
extern char __eh_frame_hdr_end;

int main() {
  int EhFrameSize = __eh_frame_end - __eh_frame_start;
  int EhFrameHdrSize = __eh_frame_hdr_end - __eh_frame_hdr_start;
  return EhFrameSize + EhFrameHdrSize;
}
