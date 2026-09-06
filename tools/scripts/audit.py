#!/usr/bin/env python3
"""Check owned C conventions and analyze each configured C compilation variant.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import concurrent.futures
import json
import pathlib
import re
import shlex
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]


def owned_files():
    return sorted(p for d in ('src', 'include', 'lib', 'samples', 'tests', 'tools')
                  for p in (ROOT / d).rglob('*') if p.suffix in ('.c', '.h')
                  and 'musashi' not in p.relative_to(ROOT).parts
                  and 'data' not in p.relative_to(ROOT).parts)


def standards(files):
    errors = []
    for path in files:
        text = path.read_text()
        name = str(path.relative_to(ROOT))
        if not text.startswith('/*') or 'MIT License' not in text.split('*/')[0]:
            errors.append(name + ': missing license/file header')
        if not re.fullmatch(r'[a-z][a-z0-9_]*\.[ch]', path.name):
            errors.append(name + ': filename is not lowercase snake_case')
        if '\t' in text:
            errors.append(name + ': tab indentation')
        code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', '', text,
                      flags=re.S)
        if re.search(r'\b_(?:aes|vdi|gem)\w*', code):
            errors.append(name + ': reserved private symbol prefix')
        if 'include' in path.relative_to(ROOT).parts:
            for match in re.finditer(
                r'(?m)^[A-Za-z_]\w*(?:[ \t*]+\w+)*[ \t*]+([a-zA-Z_]\w*)'
                r'\s*\([^;{}]*\);', text):
                if not text[:match.start()].rstrip().endswith('*/'):
                    errors.append(name + ': undocumented ' + match[1])
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=pathlib.Path, default=ROOT / 'build')
    parser.add_argument('--standards-only', action='store_true')
    args = parser.parse_args()
    build = args.build.resolve()
    output = build / 'audit' / 'scan'
    output.mkdir(parents=True, exist_ok=True)
    files = owned_files()
    errors = standards(files)
    print(f'Standards: {len(files)} owned C/header files; {len(errors)} violations')
    for error in errors:
        print(error)
    if args.standards_only:
        return bool(errors)
    compiler = shutil.which('clang-18') or shutil.which('clang')
    if not compiler:
        raise SystemExit('Install clang-18 or use tools/scripts/container.sh audit')
    commands = json.loads((build / 'compile_commands.json').read_text())
    entries = {}
    for entry in commands:
        arguments = shlex.split(entry['command'])
        normalized = []
        skip = False
        for argument in arguments:
            if skip:
                skip = False
                continue
            if argument == '-o':
                skip = True
            else:
                normalized.append(argument)
        entries[(entry['file'], tuple(normalized))] = entry

    def analyze(pair):
        index, entry = pair
        original = shlex.split(entry['command'])[1:]
        arguments = []
        skip = False
        for argument in original:
            if skip:
                skip = False
                continue
            if argument == '-o':
                skip = True
            elif argument != '-c' and not argument.startswith('-fsanitize='):
                arguments.append(argument)
        command = [compiler, '--analyze', '-Xanalyzer', '-analyzer-output=plist',
                   '-o', str(output / f'{index}.plist'), *arguments]
        try:
            result = subprocess.run(command, cwd=entry['directory'], text=True,
                                    capture_output=True, timeout=120)
            status, log = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            status, log = 124, 'Analysis timed out'
        (output / f'{index}.log').write_text(log)
        return dict(file=entry['file'], command=entry['command'],
                    status=status, output=log)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(analyze, enumerate(entries.values())))
    failures = sum(r['status'] != 0 for r in results)
    warnings = sum(r['output'].count('warning:') for r in results)
    report = dict(owned_files=len(files), standards=errors,
                  translation_units=len(results),
                  unique_files=len({r['file'] for r in results}), failures=failures,
                  warnings=warnings, results=results)
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Analyzer: {len(results)} compilation variants; {failures} failures; '
          f'{warnings} diagnostics requiring review')
    print('Evidence:', output)
    return bool(errors or failures or warnings)


if __name__ == '__main__':
    raise SystemExit(main())
