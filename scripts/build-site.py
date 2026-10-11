#!/usr/bin/env python3
"""Build the standalone educational site without changing the live C++ dashboard."""
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
output = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / '.runtime' / 'site'
if output == ROOT or ROOT in output.parents and '.runtime' not in output.relative_to(ROOT).parts:
    raise SystemExit('Choose an output outside source files, such as .runtime/site')
output.mkdir(parents=True, exist_ok=True)
for name in ['index.html', 'style.css', 'app.js', 'simulator.js']:
    shutil.copyfile(ROOT / 'dashboard' / name, output / name)
shutil.copyfile(ROOT / 'reports' / 'benchmark-10000.json', output / 'benchmarks')
(output / '.nojekyll').touch()
print(f'Standalone educational site built at {output}')
