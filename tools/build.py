#!/usr/bin/env python3

import os
import sys
import shutil
import platform
import argparse
import subprocess
from pathlib import Path
from typing import List, NoReturn, Dict

from rich.console import Console

console = Console()
ROOT: Path = Path(__file__).resolve().parent.parent
ABI_MAP: Dict[str, str] = {"arm64": "arm64-v8a", "arm32": "armeabi-v7a"}


def abort(message: str) -> NoReturn:
    console.print(f"[bold red]FATAL:[/bold red] {message}")
    sys.exit(1)


def log_step(step: str, message: str, color: str = "cyan") -> None:
    console.print(
        f"[bold blue]::[/bold blue] [{color}]{step.upper():<10}[/{color}] {message}"
    )


def set_ndk() -> None:
    if "ANDROID_NDK_HOME" in os.environ:
        return

    home = Path.home()
    system = platform.system()
    if system == "Windows":
        sdk_root = home / "AppData" / "Local" / "Android" / "Sdk"
    elif system == "Darwin":
        sdk_root = home / "Library" / "Android" / "sdk"
    else:
        sdk_root = home / "Android" / "Sdk"

    ndk_root = sdk_root / "ndk"
    if ndk_root.exists() and ndk_root.is_dir():
        versions = sorted([d for d in ndk_root.iterdir() if d.is_dir()], reverse=True)
        if versions:
            latest = str(versions[0].resolve())
            os.environ["ANDROID_NDK_HOME"] = latest
            log_step("WARN", f"ndk auto-detected at {latest}", "yellow")
            return

    abort("missing ANDROID_NDK_HOME environment variable")


def verify_deps() -> None:
    if not shutil.which("cmake"):
        abort("cmake executable not found")
    set_ndk()


def parse_stream(line: str) -> str:
    text = line.strip()
    if text.startswith("[") and "]" in text:
        idx = text.split("]")[0] + "]"
        if "Building" in text:
            name = Path(text.split()[-1]).name.replace(".o", "")
            return f"{idx} {name}"
        if "Linking" in text:
            name = Path(text.split()[-1]).name
            return f"{idx} link {name}"
    return ""


def exec_cmd(command: List[str], step_name: str) -> None:
    try:
        proc = subprocess.Popen(
            command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True,
        )
        if proc.stdout:
            for line in proc.stdout:
                message = parse_stream(line)
                if message:
                    log_step(step_name, message)
        proc.wait()
        if proc.returncode != 0:
            abort("build step failed check system logs")
    except FileNotFoundError:
        abort("executable missing during build step")


def rm_workspace(abi: str = None, variant: str = None) -> None:
    if abi and variant:
        target = ROOT / "build" / ABI_MAP[abi] / variant
    else:
        target = ROOT / "build"
    if target.exists():
        shutil.rmtree(target, ignore_errors=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="PGOV Android Build System",
        formatter_class=lambda prog: argparse.HelpFormatter(prog, max_help_position=40),
    )
    parser.add_argument(
        "-a",
        "--abi",
        type=str,
        choices=list(ABI_MAP.keys()),
        metavar="ARCH",
        help="Target architecture: arm64, arm32",
    )
    parser.add_argument(
        "-t",
        "--type",
        type=str,
        choices=["debug", "release"],
        metavar="VAR",
        help="Target build variant: debug, release",
    )
    parser.add_argument(
        "--api",
        type=int,
        default=30,
        metavar="API",
        help="Target Android API level specification",
    )
    parser.add_argument(
        "-c",
        "--clean",
        action="store_true",
        help="Force wipe build directory before compiling or globally",
    )

    args = parser.parse_args()

    if args.clean and not args.abi and not args.type:
        rm_workspace()
        log_step("CLEAN", "global build workspace wiped", "green")
        sys.exit(0)

    if not args.abi or not args.type:
        parser.error(
            "required arguments missing: --abi, --type. exception: standalone --clean"
        )

    verify_deps()

    preset = f"{args.abi}-{args.type}"

    log_step("INIT", "setup environment")

    if args.clean:
        rm_workspace(args.abi, args.type)
        log_step("CLEAN", "workspace wiped")

    log_step("CONFIG", "generating ninja files")
    cmd_conf = ["cmake", "--preset", preset, f"-DANDROID_PLATFORM=android-{args.api}"]
    exec_cmd(cmd_conf, "CONFIG")

    log_step("COMPILE", "starting build")
    cmd_comp = ["cmake", "--build", f"--preset={preset}"]
    exec_cmd(cmd_comp, "COMPILE")

    log_step("DONE", "build finished", "green")


if __name__ == "__main__":
    main()
