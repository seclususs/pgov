#!/usr/bin/env python3

import shutil
from pathlib import Path
from typing import List


def mv_files(root: Path, files: List[str]) -> None:
    for f in files:
        src: Path = root / f
        dst: Path = root / src.name
        if src.exists():
            shutil.move(str(src), str(dst))


def rm_paths(root: Path, paths: List[str]) -> None:
    for p in paths:
        tgt: Path = root / p
        if tgt.is_file():
            tgt.unlink()
        elif tgt.is_dir():
            shutil.rmtree(str(tgt))


def main() -> None:
    root: Path = Path(__file__).resolve().parent.parent

    mv_list: List[str] = [
        "android/Android.bp",
        "android/file_contexts",
        "android/pgovd.rc",
        "android/pgovd.te",
    ]

    rm_list: List[str] = [
        "android",
        "CMakeLists.txt",
        "CMakePresets.json",
        "modules",
        ".github",
        "tools",
        ".clang-format",
        ".clang-tidy",
        ".editorconfig",
        ".gitattributes",
        ".gitignore",
    ]

    mv_files(root, mv_list)
    rm_paths(root, rm_list)


if __name__ == "__main__":
    main()
