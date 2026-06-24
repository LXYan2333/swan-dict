#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]

PATCHED_FILES = [
    ("ui/DigitalClock.qml", "0001-digital-clock-qml-date-label.patch"),
    ("ui/Tooltip.qml", "0002-tooltip-qml-dictionary-content.patch"),
    ("ui/main.qml", "0003-main-qml-swan-dict-wiring.patch"),
    ("config/main.xml", "0005-main-xml-translation-settings.patch"),
    ("ui/configAppearance.qml", "0006-config-appearance-digital-clock-i18n.patch"),
    ("ui/configCalendar.qml", "0007-config-calendar-digital-clock-i18n.patch"),
    ("ui/configTimeZones.qml", "0008-config-time-zones-digital-clock-i18n.patch"),
    ("ui/CalendarView.qml", "0009-calendar-view-digital-clock-i18n.patch"),
    ("ui/NoTimezoneWarning.qml", "0010-no-timezone-warning-digital-clock-i18n.patch"),
]

PROFILE_DISTROS = {
    "debian/13": "debian",
    "ubuntu/26.04": "ubuntu",
    "fedora/44": "fedora",
    "arch/latest": "arch",
}


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def run(command: list[str], cwd: Path | None = None, stdout=None) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(command, cwd=cwd, check=True, stdout=stdout)
    except FileNotFoundError:
        fail(f"Required command not found: {command[0]}")
    except subprocess.CalledProcessError as error:
        fail(f"Command failed with exit code {error.returncode}: {' '.join(command)}")


def require_command(command: str, hint: str) -> None:
    if shutil.which(command) is None:
        fail(f"Required command not found: {command}. {hint}")


def env_path(name: str, default: Path | None = None) -> Path | None:
    value = os.environ.get(name)
    if value:
        return Path(value).expanduser().resolve()
    return default


def env_flag(name: str) -> bool:
    return os.environ.get(name) == "1"


def selected_profile() -> str:
    profile = os.environ.get("SWAN_DICT_PROFILE", "debian/13")
    if profile not in PROFILE_DISTROS:
        known = ", ".join(sorted(PROFILE_DISTROS))
        fail(f"Unsupported SWAN_DICT_PROFILE={profile}. Known profiles: {known}")
    return profile


def source_cache_base() -> Path:
    return env_path("SWAN_DICT_SOURCE_CACHE", REPO_ROOT / ".cache" / "plasma-workspace-source")


def source_cache_root(profile: str) -> Path:
    return source_cache_base() / profile


def prepared_source_root(profile: str) -> Path:
    return source_cache_root(profile) / "source"


def download_work_root(profile: str) -> Path:
    return source_cache_root(profile) / "download"


def applet_root(profile: str) -> Path:
    return env_path("SWAN_DICT_APPLET_ROOT", REPO_ROOT / "applets" / profile)


def digital_clock_patch_dir(profile: str) -> Path:
    return env_path("SWAN_DICT_DIGITAL_CLOCK_PATCH_DIR", REPO_ROOT / "patches" / "digital-clock" / profile)


def source_file(source_ui: Path, source_config: Path, relative_name: str) -> Path:
    if relative_name.startswith("config/"):
        return source_config / relative_name.removeprefix("config/")
    return source_ui / relative_name.removeprefix("ui/")


def has_generated_applet_files(target_contents: Path) -> bool:
    if (target_contents / "config").is_dir():
        return True
    ui_dir = target_contents / "ui"
    return ui_dir.is_dir() and any(ui_dir.glob("*.qml"))


def copy_tree_contents(source: Path, target: Path) -> None:
    shutil.rmtree(target, ignore_errors=True)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target, symlinks=True)


def copy_prepared_source(source: Path, target: Path) -> None:
    if not source.is_dir():
        fail(f"Plasma Workspace source tree not found: {source}")
    if not find_digital_clock_source(source, fail_if_missing=False):
        fail(f"Digital Clock source not found in Plasma Workspace source tree: {source}")
    copy_tree_contents(source, target)


def extract_tarball(archive: Path, target: Path) -> Path:
    shutil.rmtree(target, ignore_errors=True)
    target.mkdir(parents=True, exist_ok=True)
    run(["tar", "-C", str(target), "-xf", str(archive)])
    dirs = [item for item in target.iterdir() if item.is_dir()]
    if len(dirs) == 1:
        return dirs[0]
    for item in dirs:
        if find_digital_clock_source(item, fail_if_missing=False):
            return item
    fail(f"Unable to identify extracted Plasma Workspace source directory from: {archive}")


def prepare_deb_source(profile: str, distro: str) -> None:
    require_command("apt-get", f"{distro} source package workflow requires apt-get.")
    work = download_work_root(profile)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(["apt-get", "source", "plasma-workspace"], cwd=work, check=False)
    if result.returncode != 0:
        fail(
            f"{distro} source package for plasma-workspace could not be downloaded. "
            "Enable source repositories for this distro, then rerun prepare-source."
        )
    candidates = [item for item in work.iterdir() if item.is_dir() and item.name.startswith("plasma-workspace")]
    if not candidates:
        fail(f"{distro} source package downloaded but no unpacked plasma-workspace source directory was found.")
    source = sorted(candidates)[0]
    copy_prepared_source(source, prepared_source_root(profile))


def prepare_fedora_source(profile: str) -> None:
    require_command("dnf", "Fedora source package workflow requires dnf.")
    require_command("rpm2cpio", "Fedora source package workflow requires rpm tools.")
    require_command("cpio", "Fedora source package workflow requires cpio.")
    work = download_work_root(profile)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(["dnf", "download", "--source", "--destdir", str(work), "plasma-workspace"], check=False)
    if result.returncode != 0:
        fail("Fedora source package for plasma-workspace could not be downloaded. Install dnf-plugins-core and enable source repositories.")

    srpms = sorted(work.glob("*.src.rpm"))
    if not srpms:
        fail("Fedora source package download finished but no .src.rpm was found.")

    extracted = work / "srpm"
    extracted.mkdir(parents=True, exist_ok=True)
    rpm2cpio = subprocess.Popen(["rpm2cpio", str(srpms[0])], stdout=subprocess.PIPE)
    cpio = subprocess.run(["cpio", "-id"], cwd=extracted, stdin=rpm2cpio.stdout, check=False)
    if rpm2cpio.stdout:
        rpm2cpio.stdout.close()
    rpm_result = rpm2cpio.wait()
    if rpm_result != 0 or cpio.returncode != 0:
        fail("Failed to extract Fedora plasma-workspace source RPM. Install rpm tools and cpio.")

    archives = sorted(extracted.glob("plasma-workspace*.tar.*"))
    if not archives:
        fail("Fedora plasma-workspace source RPM did not contain a plasma-workspace tarball.")
    source = extract_tarball(archives[0], work / "unpacked")
    copy_prepared_source(source, prepared_source_root(profile))


def prepare_arch_source(profile: str) -> None:
    require_command("git", "Arch source package workflow requires git.")
    require_command("makepkg", "Arch source package workflow requires makepkg.")
    work = download_work_root(profile)
    work.mkdir(parents=True, exist_ok=True)
    package_dir = work / "plasma-workspace"
    if package_dir.exists():
        shutil.rmtree(package_dir)
    repo_url = os.environ.get("SWAN_DICT_ARCH_PACKAGE_REPO_URL", "https://gitlab.archlinux.org/archlinux/packaging/packages/plasma-workspace.git")
    result = subprocess.run(["git", "clone", "--depth=1", repo_url, str(package_dir)], cwd=work, check=False)
    if result.returncode != 0:
        fail(f"Arch source package recipe for plasma-workspace could not be cloned from {repo_url}.")
    run(["makepkg", "--nobuild", "--nodeps", "--skippgpcheck"], cwd=package_dir)
    source_root = package_dir / "src"
    if not source_root.is_dir():
        fail("Arch plasma-workspace source recipe did not create a src directory.")
    source = find_plasma_workspace_source(source_root)
    copy_prepared_source(source, prepared_source_root(profile))


def find_plasma_workspace_source(root: Path) -> Path:
    if find_digital_clock_source(root, fail_if_missing=False):
        return root
    candidates = [item for item in root.iterdir() if item.is_dir() and item.name.startswith("plasma-workspace")]
    for candidate in sorted(candidates):
        if find_digital_clock_source(candidate, fail_if_missing=False):
            return candidate
    fail(f"No plasma-workspace source tree with Digital Clock source was found under: {root}")


def prepare_source() -> None:
    profile = selected_profile()
    source_override = env_path("SWAN_DICT_PLASMA_WORKSPACE_SOURCE_DIR")
    target = prepared_source_root(profile)

    if target.exists() and not env_flag("SWAN_DICT_PREPARE_SOURCE_OVERWRITE") and source_override is None:
        print(f"Prepared source already exists: {target}")
        return

    if source_override is not None:
        copy_prepared_source(source_override, target)
        print(f"Prepared {profile} source from: {source_override}")
        return

    distro = PROFILE_DISTROS[profile]
    if distro in {"debian", "ubuntu"}:
        prepare_deb_source(profile, distro)
    elif distro == "fedora":
        prepare_fedora_source(profile)
    elif distro == "arch":
        prepare_arch_source(profile)
    else:
        fail(f"{profile} does not have a supported distro source package workflow.")
    print(f"Prepared {profile} source: {target}")


def find_digital_clock_source(source_dir: Path, fail_if_missing: bool = True) -> Path | None:
    candidates: list[Path] = []
    for path in source_dir.rglob("applets/digital-clock"):
        packaged = path / "package" / "contents"
        if (packaged / "ui" / "DigitalClock.qml").is_file() or (packaged / "DigitalClock.qml").is_file():
            candidates.append(packaged)
        if (path / "DigitalClock.qml").is_file() or (path / "ui" / "DigitalClock.qml").is_file():
            candidates.append(path)

    if candidates:
        return sorted(candidates, key=lambda item: len(item.parts))[0]
    if fail_if_missing:
        fail(f"Digital Clock source not found under prepared Plasma Workspace source: {source_dir}")
    return None


def resolve_source_layout(source_root: Path) -> tuple[Path, Path]:
    if (source_root / "ui").is_dir():
        return source_root / "ui", source_root / "config"
    return source_root, source_root


def prepared_digital_clock_source(profile: str) -> Path:
    source_root = env_path("SWAN_DICT_DIGITAL_CLOCK_SOURCE")
    if source_root is not None:
        if not source_root.is_dir():
            fail(f"Explicit Digital Clock source not found: {source_root}")
        return source_root

    source_root = prepared_source_root(profile)
    if not source_root.is_dir():
        fail(f"Prepared source not found for {profile}: {source_root}. Run: SWAN_DICT_PROFILE={profile} python3 scripts/manage.py prepare-source")
    digital_clock_source = find_digital_clock_source(source_root)
    assert digital_clock_source is not None
    return digital_clock_source


def copy_digital_clock(source_root: Path, target_contents: Path) -> None:
    source_ui, source_config = resolve_source_layout(source_root)
    target_ui = target_contents / "ui"
    target_config = target_contents / "config"

    shutil.rmtree(target_ui, ignore_errors=True)
    shutil.rmtree(target_config, ignore_errors=True)
    target_ui.mkdir(parents=True, exist_ok=True)
    target_config.mkdir(parents=True, exist_ok=True)

    nested_config = source_config / "config"
    if nested_config.is_dir():
        shutil.copytree(nested_config, target_config, dirs_exist_ok=True)
    else:
        for name in ("config.qml", "main.xml"):
            source = source_config / name
            if not source.is_file():
                fail(f"Digital Clock config file not found: {source}")
            shutil.copy2(source, target_config / name)

    for source in sorted(source_ui.glob("*.qml")):
        if source.name == "config.qml":
            continue
        shutil.copy2(source, target_ui / source.name)


def apply_patches(patch_dir: Path, target_applet: Path) -> None:
    patches = sorted(patch_dir.glob("*.patch"))
    if not patches:
        fail(f"No patch files found in: {patch_dir}")
    for patch_file in patches:
        with patch_file.open("rb") as patch_input:
            subprocess.run(["patch", "-d", str(target_applet), "-p1"], stdin=patch_input, check=True)


def copy_owned_applet_files(target_applet: Path) -> None:
    target_contents = target_applet / "contents"
    owned_config = REPO_ROOT / "applet-owned" / "config" / "config.qml"
    common_applet = REPO_ROOT / "applets" / "common"

    if owned_config.is_file():
        (target_contents / "config").mkdir(parents=True, exist_ok=True)
        shutil.copy2(owned_config, target_contents / "config" / "config.qml")

    common_metadata = common_applet / "metadata.json"
    if common_metadata.is_file():
        target_applet.mkdir(parents=True, exist_ok=True)
        shutil.copy2(common_metadata, target_applet / "metadata.json")

    common_ui = common_applet / "contents" / "ui"
    if common_ui.is_dir():
        target_ui = target_contents / "ui"
        target_ui.mkdir(parents=True, exist_ok=True)
        for source in sorted(common_ui.iterdir()):
            if source.is_file():
                shutil.copy2(source, target_ui / source.name)


def sync_digital_clock() -> None:
    profile = selected_profile()
    target_applet = applet_root(profile)
    target_contents = target_applet / "contents"
    patch_dir = digital_clock_patch_dir(profile)

    if has_generated_applet_files(target_contents) and not env_flag("SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE"):
        print("Digital Clock generated files already exist; not overwriting.", file=sys.stderr)
        print("Set SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 to refresh them from prepared source and reapply patches.", file=sys.stderr)
        return

    source_root = prepared_digital_clock_source(profile)
    copy_digital_clock(source_root, target_contents)
    apply_patches(patch_dir, target_applet)
    copy_owned_applet_files(target_applet)


def regenerate_patches() -> None:
    profile = selected_profile()
    source_root = prepared_digital_clock_source(profile)
    current_contents = applet_root(profile) / "contents"
    patch_dir = digital_clock_patch_dir(profile)

    if not current_contents.is_dir():
        fail(f"Current applet contents not found: {current_contents}")

    source_ui, source_config = resolve_source_layout(source_root)
    patch_dir.mkdir(parents=True, exist_ok=True)

    for relative_name, patch_name in PATCHED_FILES:
        upstream = source_file(source_ui, source_config, relative_name)
        current = current_contents / relative_name
        patch_file = patch_dir / patch_name

        if not upstream.is_file():
            fail(f"Upstream file not found: {upstream}")
        if not current.is_file():
            fail(f"Current file not found: {current}")

        if upstream.read_bytes() == current.read_bytes():
            if patch_file.exists():
                patch_file.unlink()
                print(f"removed: {patch_file}")
            else:
                print(f"unchanged: {relative_name}")
            continue

        generated = subprocess.run(
            [
                "diff",
                "-u",
                "--label",
                f"upstream/contents/{relative_name}",
                "--label",
                f"current/contents/{relative_name}",
                str(upstream),
                str(current),
            ],
            check=False,
            stdout=subprocess.PIPE,
        )
        if generated.returncode not in (0, 1):
            fail(f"diff failed for: {relative_name}")

        patch_bytes = generated.stdout
        if patch_file.exists() and patch_file.read_bytes() == patch_bytes:
            print(f"unchanged: {patch_file}")
        else:
            patch_file.write_bytes(patch_bytes)
            print(f"updated: {patch_file}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Swan Dict project management commands")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("prepare-source", help="download and unpack the distro plasma-workspace source package for the selected profile")
    subparsers.add_parser("sync-digital-clock", help="copy prepared Digital Clock source and apply the selected profile patches")
    subparsers.add_parser("regenerate-patches", help="regenerate selected profile patches from edited generated applet files")
    args = parser.parse_args()

    if args.command == "prepare-source":
        prepare_source()
    elif args.command == "sync-digital-clock":
        sync_digital_clock()
    elif args.command == "regenerate-patches":
        regenerate_patches()


if __name__ == "__main__":
    main()
