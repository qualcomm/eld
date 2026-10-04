#!/usr/bin/env python3
# Usage: make_tar.py FILE OUTPUT
# Writes OUTPUT as a tar archive holding FILE, like `tar cf OUTPUT FILE`.
import os
import sys
import tarfile

src, out = sys.argv[1:]
with tarfile.open(out, "w", format=tarfile.GNU_FORMAT) as tar:
    tar.add(src, arcname=os.path.basename(src))
