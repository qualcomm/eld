llvm_add_library(
  ELDFragment
  STATIC
  BuildIDFragment.cpp
  EhFrameFragment.cpp
  EhFrameHdrFragment.cpp
  FillFragment.cpp
  Fragment.cpp
  FragmentRef.cpp
  FragUtils.cpp
  GNUHashFragment.cpp
  GNUVerDefFragment.cpp
  GNUVerSymFragment.cpp
  GOT.cpp
  MergeStringFragment.cpp
  OutputSectDataFragment.cpp
  PLT.cpp
  RegionFragment.cpp
  RegionFragmentEx.cpp
  RegionTableFragment.cpp
  StringFragment.cpp
  Stub.cpp
  SysVHashFragment.cpp
  TargetFragment.cpp
  TimingFragment.cpp)

target_link_libraries(ELDFragment PRIVATE ELDSymbolResolver)
