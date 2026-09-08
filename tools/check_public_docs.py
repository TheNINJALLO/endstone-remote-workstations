"""Validate local Markdown links and Python snippets in the public documentation."""
import ast
from pathlib import Path
import re
import sys
from urllib.parse import unquote
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    paths = [ROOT/'README.md',ROOT/'CONTRIBUTING.md',ROOT/'CHANGELOG.md',
             *sorted((ROOT/'docs').rglob('*.md')),*sorted((ROOT/'examples').rglob('README.md'))]
    missing, checked, snippets = [],0,0
    # Historical research paths in prose are deliberately private. Actual local
    # Markdown/HTML links must resolve in the published tree.
    for path in paths:
        text = path.read_text(encoding='utf-8')
        links = re.findall(r'\]\(([^)]+)\)',text)+re.findall(r'(?:src|href)="([^"]+)"',text)
        for link in links:
            if link.startswith(('https://','http://','#','mailto:')):
                continue
            link = unquote(link.split('#')[0].split(' "')[0].strip('<>'))
            if link and not (path.parent/link).exists():
                missing.append(f'{path.relative_to(ROOT)}: {link}')
            checked += 1
        if path.name in ('README.md','EXAMPLES.md','DEVELOPER_API.md'):
            for snippet in re.findall(r'```python\s*\n(.*?)```',text,re.S):
                ast.parse(snippet)
                snippets += 1
    ET.parse(ROOT/'docs/assets/banner.svg')
    if missing:
        print('\n'.join(missing))
        raise SystemExit('Broken local documentation links')
    print(f'{checked} local links and {snippets} Python snippets checked; banner SVG parsed.')


if __name__ == '__main__':
    main()
