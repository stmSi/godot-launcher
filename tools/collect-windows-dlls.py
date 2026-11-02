#!/usr/bin/env python3
"""Copy non-system MinGW runtime dependencies beside a Windows executable."""

import pathlib
import re
import shutil
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
prefix = pathlib.Path(sys.executable).parent
pending = [executable]
seen = set()
while pending:
    binary = pending.pop()
    output = subprocess.check_output(["objdump", "-p", str(binary)], text=True)
    for name in re.findall(r"DLL Name:\s*(\S+)", output):
        if name.lower() in seen:
            continue
        seen.add(name.lower())
        source = prefix / name
        if source.exists():
            destination = executable.parent / name
            shutil.copy2(source, destination)
            pending.append(destination)
