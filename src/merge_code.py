#!/usr/bin/env python3
"""
Merge code files in a directory into a single organized Markdown document.
"""

import argparse
import re
from pathlib import Path

# Mapping file extensions to markdown language identifiers
EXTENSION_MAP = {
    ".cpp": "cpp",
    ".hpp": "cpp",
    ".cc": "cpp",
    ".cxx": "cpp",
    ".c": "c",
    ".h": "c",
    ".py": "python",
    ".js": "javascript",
    ".ts": "typescript",
    ".jsx": "javascript",
    ".tsx": "typescript",
    ".html": "html",
    ".css": "css",
    ".json": "json",
    ".yaml": "yaml",
    ".yml": "yaml",
    ".xml": "xml",
    ".sh": "bash",
    ".bash": "bash",
    ".rs": "rust",
    ".go": "go",
    ".java": "java",
    ".cs": "csharp",
    ".glsl": "glsl",
    ".vert": "glsl",
    ".frag": "glsl",
    ".comp": "glsl",
    ".sql": "sql",
    ".cmake": "cmake",
}

# Common binary or non-code extensions to ignore
IGNORE_EXTENSIONS = {
    ".pyc", ".pyo", ".exe", ".o", ".obj", ".dll", ".so", ".a", ".lib",
    ".png", ".jpg", ".jpeg", ".gif", ".ico", ".pdf", ".zip", ".tar", ".gz"
}


def get_language(file_path: Path) -> str:
    """Return markdown code block language for a given file path."""
    return EXTENSION_MAP.get(file_path.suffix.lower(), "")


def get_fence(content: str) -> str:
    """Determine markdown fence backticks required to wrap content without breaking syntax."""
    max_count = 0
    count = 0
    for char in content:
        if char == '`':
            count += 1
            max_count = max(max_count, count)
        else:
            count = 0
    return "`" * max(3, max_count + 1)


def make_anchor(header_text: str) -> str:
    """Convert header text into a GitHub-compatible markdown anchor."""
    anchor = header_text.lower()
    anchor = re.sub(r'[^a-z0-9\s-]', '', anchor)
    anchor = re.sub(r'[\s]+', '-', anchor)
    return anchor


def merge_files(directory: Path, output_file: Path) -> None:
    """Read all code files in directory and write to output markdown file."""
    script_path = Path(__file__).resolve()
    output_path = output_file.resolve()

    files = []
    for entry in sorted(directory.iterdir()):
        if not entry.is_file():
            continue
        # Skip the script itself and the destination file
        if entry.resolve() in (script_path, output_path):
            continue
        if entry.name.startswith("."):
            continue
        if entry.suffix.lower() in IGNORE_EXTENSIONS:
            continue
        files.append(entry)

    if not files:
        print(f"No eligible code files found in '{directory}'.")
        return

    out_lines = []
    out_lines.append("# Codebase Summary\n")
    out_lines.append(f"Merged `{len(files)}` files from `{directory.resolve().name}`.\n")
    out_lines.append("## Table of Contents\n")

    file_data = []

    for file_path in files:
        try:
            content = file_path.read_text(encoding="utf-8", errors="replace")
        except Exception as e:
            print(f"Skipping {file_path.name}: {e}")
            continue

        line_count = len(content.splitlines())
        file_size = file_path.stat().st_size
        file_data.append((file_path, content, line_count, file_size))

        anchor = make_anchor(file_path.name)
        out_lines.append(f"- [{file_path.name}](#{anchor}) (`{line_count}` lines)")

    out_lines.append("\n---\n")

    for file_path, content, line_count, file_size in file_data:
        lang = get_language(file_path)
        fence = get_fence(content)

        out_lines.append(f"## {file_path.name}\n")
        out_lines.append(f"**Path:** `{file_path.name}` | **Lines:** {line_count} | **Size:** {file_size} bytes\n")
        out_lines.append(f"{fence}{lang}")
        out_lines.append(content.rstrip())
        out_lines.append(f"{fence}\n")

    markdown_content = "\n".join(out_lines) + "\n"
    output_path.write_text(markdown_content, encoding="utf-8")

    total_lines = len(markdown_content.splitlines())
    print(f"Successfully merged {len(file_data)} files ({total_lines} lines) into '{output_file.name}'.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge codebase files into an organized Markdown file.")
    parser.add_argument(
        "-d", "--directory",
        type=Path,
        default=Path(__file__).parent,
        help="Directory containing code files (default: directory of script)"
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        default=Path("merged_code.md"),
        help="Output Markdown filename (default: merged_code.md)"
    )

    args = parser.parse_args()
    merge_files(args.directory, args.output)


if __name__ == "__main__":
    main()
