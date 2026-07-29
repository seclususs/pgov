#!/usr/bin/env python3

import logging as log
import os
import shutil
import sys
import zipfile
from pathlib import Path

log.basicConfig(
    level=log.INFO,
    format="%(message)s",
    stream=sys.stdout,
)


def read_version(prop_path: Path) -> str:
    if not prop_path.exists():
        log.error(f"  ERR      missing {prop_path.name}")
        sys.exit(1)

    with open(prop_path, "r", encoding="utf-8") as file:
        for line in file:
            if line.strip().startswith("version="):
                return line.strip().split("=", 1)[1].strip()

    log.error("  ERR      version= undefined in module.prop")
    sys.exit(1)


def verify_artifacts(artifacts: list[Path]) -> None:
    missing = [path for path in artifacts if not path.is_file()]
    if missing:
        for path in missing:
            log.error(f"  ERR      missing bin: {path.parent.parent.name}/{path.name}")
        sys.exit(1)


def stage_binaries(targets: dict[Path, Path]) -> None:
    for src, dst in targets.items():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        log.info(f"  STAGE     {dst.parent.name}/{dst.name}")


def clean_staged_binaries(staged_files: list[Path]) -> None:
    for path in staged_files:
        if path.exists():
            path.unlink()
            log.info(f"  CLEAN     {path.parent.name}/{path.name}")


def create_archive(source_dir: Path, output_zip: Path) -> None:
    with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for root, _, files in os.walk(source_dir):
            root_path = Path(root)
            for file_name in sorted(files):
                if file_name == ".gitkeep":
                    continue
                file_path = root_path / file_name
                archive.write(
                    file_path,
                    arcname=file_path.relative_to(source_dir),
                )


def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    modules_dir = project_root / "modules"
    module_prop = modules_dir / "module.prop"
    dist_dir = project_root / "dist"
    dist_dir.mkdir(parents=True, exist_ok=True)

    version = read_version(module_prop)
    output_zip = dist_dir / f"pgovd-{version}.zip"

    arm64_src = project_root / "build" / "arm64-v8a" / "release" / "pgovd"
    arm32_src = project_root / "build" / "armeabi-v7a" / "release" / "pgovd"

    arm64_dst = modules_dir / "system" / "bin" / "arm64-v8a" / "pgovd"
    arm32_dst = modules_dir / "system" / "bin" / "armeabi-v7a" / "pgovd"

    verify_artifacts([arm64_src, arm32_src])

    staging_map = {
        arm64_src: arm64_dst,
        arm32_src: arm32_dst,
    }

    try:
        stage_binaries(staging_map)
        if output_zip.exists():
            output_zip.unlink()

        log.info(f"  ZIP       {output_zip.name}")
        create_archive(modules_dir, output_zip)
        log.info(f"  DONE      {output_zip}")
    finally:
        clean_staged_binaries([arm64_dst, arm32_dst])


if __name__ == "__main__":
    main()
