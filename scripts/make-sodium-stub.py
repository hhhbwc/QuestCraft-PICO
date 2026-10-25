#!/usr/bin/env python3
"""Write the inert Sodium placeholder jar (keeps the filename so the launcher
does not re-download the real mod). See KNOWN-ISSUES #2."""
import json, sys, zipfile

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'Sodium.jar'
    meta = {
        "schemaVersion": 1,
        "id": "qcxr-sodium-off",
        "version": "0.0.0",
        "name": "Sodium (disabled for MobileGlues)",
        "environment": "client",
        "description": "Placeholder that keeps the filename so the launcher does not re-download Sodium."
    }
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('fabric.mod.json', json.dumps(meta, indent=2))
    print('wrote', out)

if __name__ == '__main__':
    main()
