#!/usr/bin/env python3
"""Inject the Android OpenXR binding into a built mod jar.

Loom refuses to nest a plain file dependency ("not a module component and has no
capabilities"), but fabric.mod.json has to list it under "jars", so we add it here.
Usage: python inject-openxr-jar.py <mod.jar> <lwjgl-openxr-android.jar> [entry-name]
"""
import os, sys, zipfile

def main():
    if len(sys.argv) < 3:
        print(__doc__); return 1
    mod, binding = sys.argv[1], sys.argv[2]
    entry = sys.argv[3] if len(sys.argv) > 3 else 'META-INF/jars/' + os.path.basename(binding)
    tmp = mod + '.tmp'
    data = open(binding, 'rb').read()
    with zipfile.ZipFile(mod) as zin, zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as zout:
        zout.writestr(entry, data)
        for item in zin.infolist():
            if item.filename == entry or item.filename.endswith('/'):
                continue
            zout.writestr(item, zin.read(item.filename))
    os.replace(tmp, mod)
    print(f'injected {entry} into {mod}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
