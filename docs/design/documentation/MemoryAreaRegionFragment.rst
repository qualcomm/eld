Memory Area Region Fragment
============================

.. contents::
   :local:

Information
-----------------
All input files that are placed on command line (scripts, archives and
relocatables), have actual code/data and ELF specific structures. The actual
code/data is stored as a RegionFragment in ELD. This fragment has a member
StringRef which is raw data of file region that the fragment represents.

The actual use of the raw data is only during creation of IR such as Relocation
fragment, symbol tables, Input object creation. The RegionFragment is a read
only memory, and is used along with linker generated data (symbols, applied
relocation entries etc) when generationg out put file. This means there is a
waste of memory to keep the raw data throughtout the linking process.

A more efficient representation of the RegionFragment will help reduce
linking large code bases.

Option
--------
We will have an option called -Om that will reduce the memory usage due to
input read only raw data.

By default, this option is disabled.

Replacing raw data
-----------------------

By default the RegionFragment has an llvm::StringRef as member variable carrying
the raw data.
We can replace this with MemoryArea and abstract out if the raw data is kept on
file or in memory. The interface  MemoryArea::request will use llvm::MemoryBuffer::getFile
to open file seek a position for access by the caller.
If the memory is in-memory, the operation should be a simple return of a StringRef.

API/class changes:
-------------------
  RegionFragment:
  Region fragment will have a pointer to a MemoryArea instead of a StringRef.

  RegionFragment::getRegion:
  This will either return a StringRef or do a request and return a StringRef
  from newly created memory buffer based on -Om option.

  MemoryArea:
  The class will contain filename, offset in file where the region begins,
  the size of region and a flag indicating if this area represents a file
  or it is an "in memory" entity such as linker scripts created on the fly
  with defsym options.

  MemoryArea::request:
  API will open file and seek at offset in file + offset within region
  supplied as parameter, create a StringRef of the same and return it.

Future Enhancement(s)
---------------------
  We could use a user provided list of files to keep cached in memory.
