#!/usr/bin/env python3
import hashlib
import json
import pathlib
import sys

output = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2]).resolve()
compiler = sys.argv[3]
hash_value = hashlib.sha256(compiler.encode())
for path in sorted(pathlib.Path(name).resolve() for name in sys.argv[4:]):
    hash_value.update(str(path.relative_to(root)).encode())
    hash_value.update(b"\0")
    hash_value.update(path.read_bytes())
    hash_value.update(b"\0")
source = '#include "Gameboy.h"\n\nstd::string_view Gameboy::CoreBuildIdentity() { return ' + json.dumps(hash_value.hexdigest()) + '; }\n'
output.parent.mkdir(parents=True, exist_ok=True)
if not output.exists() or output.read_text() != source:
    output.write_text(source)
