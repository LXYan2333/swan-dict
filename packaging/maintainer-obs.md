# OBS Maintainer Commands

This document lists the `osc` commands a Swan Dict maintainer usually needs.
It assumes the OBS project is:

```text
home:lxyan3
```

and the OBS package is:

```text
swan-dict
```

## Login

Install and configure `osc` once:

```console
osc ls
```

If credentials are missing, `osc` prompts for them and writes its config under:

```text
~/.config/osc/oscrc
```

## Check Out The OBS Package

```console
mkdir -p /tmp/swan-dict-obs
cd /tmp/swan-dict-obs
osc checkout home:lxyan3 swan-dict
cd home:lxyan3/swan-dict
```

Show local OBS checkout changes:

```console
osc status
```

Update the checkout from OBS:

```console
osc update
```

## Project Repositories

Show the OBS project metadata:

```console
osc meta prj home:lxyan3
```

Edit repositories in a file:

```console
osc meta prj home:lxyan3 > /tmp/home-lxyan3.xml
```

After editing `/tmp/home-lxyan3.xml`, upload it:

```console
osc meta prj home:lxyan3 -F /tmp/home-lxyan3.xml
```

Example repository block:

```xml
<repository name="Fedora_44">
  <path project="Fedora:44" repository="standard"/>
  <arch>x86_64</arch>
</repository>
```

## Prepare Source For OBS

Before generating the OBS source archive, make sure patches and generated
fallback files are current.

For normal generated Digital Clock files from the local system:

```console
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 scripts/sync-digital-clock.sh
```

For the Arch fallback source included in the OBS tarball:

```console
SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR=/path/to/plasma-workspace \
SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 \
scripts/prepare-arch-digital-clock-fallback.sh
```

If running on Arch, the script can fetch Arch's `plasma-workspace` source recipe
itself:

```console
SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 \
scripts/prepare-arch-digital-clock-fallback.sh
```

## Generate Source Archives

The current OBS setup uses these files:

```text
PKGBUILD
swan-dict.spec
swan-dict-1.0.0.tar.xz
swan-dict_1.0.0.orig.tar.xz
swan-dict_1.0.0-1.debian.tar.xz
swan-dict_1.0.0-1.dsc
```

Create a temporary source tree:

```console
tmp_dir="$(mktemp -d /tmp/swan-dict-obs-src.XXXXXX)"
mkdir -p "${tmp_dir}/swan-dict-1.0.0"
tar -C /home/lxyan/Code/kde/swan-dict \
    --exclude='./.git' \
    --exclude='./build' \
    --exclude='./applet/contents/data/ecdict.sqlite' \
    --exclude='./third_party/ECDICT/stardict.7z' \
    --exclude='./.cache' \
    --exclude='./__pycache__' \
    -cf - . | tar -C "${tmp_dir}/swan-dict-1.0.0" -xf -
```

Create the generic tarball for RPM and Arch:

```console
tar -C "${tmp_dir}" -cJf \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/swan-dict-1.0.0.tar.xz \
    swan-dict-1.0.0
```

Create Debian source artifacts:

```console
cp -a /home/lxyan/Code/kde/swan-dict/packaging/obs/debian \
    "${tmp_dir}/swan-dict-1.0.0/debian"
tar -C "${tmp_dir}" --exclude='swan-dict-1.0.0/debian' -cJf \
    "${tmp_dir}/swan-dict_1.0.0.orig.tar.xz" \
    swan-dict-1.0.0
cd "${tmp_dir}"
dpkg-source -b swan-dict-1.0.0
cp swan-dict_1.0.0.orig.tar.xz \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/
cp swan-dict_1.0.0-1.debian.tar.xz \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/
cp swan-dict_1.0.0-1.dsc \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/
```

Copy packaging recipes into the OBS checkout:

```console
cp /home/lxyan/Code/kde/swan-dict/packaging/obs/swan-dict.spec \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/
cp /home/lxyan/Code/kde/swan-dict/packaging/obs/PKGBUILD \
    /tmp/swan-dict-obs/home:lxyan3/swan-dict/
```

Remove old versioned source files from the OBS checkout when changing versions:

```console
cd /tmp/swan-dict-obs/home:lxyan3/swan-dict
osc remove swan-dict-0.1.tar.xz
osc remove swan-dict_0.1.orig.tar.xz
osc remove swan-dict_0.1-1.debian.tar.xz
osc remove swan-dict_0.1-1.dsc
osc add swan-dict-1.0.0.tar.xz
osc add swan-dict_1.0.0.orig.tar.xz
osc add swan-dict_1.0.0-1.debian.tar.xz
osc add swan-dict_1.0.0-1.dsc
```

## Submit OBS Changes

Review changed files:

```console
cd /tmp/swan-dict-obs/home:lxyan3/swan-dict
osc status
```

Commit:

```console
osc commit -m "Release 1.0.0"
```

## GitHub Actions

The repository has a workflow at:

```text
.github/workflows/obs.yml
```

It updates the OBS package when either of these happens:

- a git tag matching `v*` is pushed, for example `v1.0.0`
- the workflow is started manually from GitHub Actions

Add these GitHub repository secrets before using it:

```text
OBS_USER
OBS_PASSWORD
```

`OBS_USER` is your OBS username. `OBS_PASSWORD` must be accepted by `osc` for
checking out and committing package sources. A limited OBS token that only
triggers rebuilds or source services is not enough for this workflow, because
the workflow uploads source archives and runs `osc commit`.

The workflow runs:

```console
packaging/obs/update-obs-sources.sh
```

That script:

- prepares the Arch Digital Clock fallback from KDE's Plasma Workspace source
- creates the generic source archive
- creates Debian source artifacts with `dpkg-source`
- copies the OBS recipe files
- removes old versioned source artifacts from the OBS checkout
- commits the OBS package when `SWAN_DICT_OBS_COMMIT=1`

Manual workflow input `version` overrides the version read from
`CMakeLists.txt`. Use it only after the package metadata files have been updated
to the same version. The release checklist below lists those files.

The workflow prints `osc results` once after committing. It does not wait for
all builds to finish, so check OBS or use the commands below for final build
status.

## Build Results

Show current results:

```console
osc results home:lxyan3 swan-dict --verbose
```

Watch until builds finish:

```console
osc results home:lxyan3 swan-dict --watch --verbose
```

For long-running or noisy builds, prefer checking the table first and only read
logs for failed targets.

## Build Logs

Show the last 30 lines for a failed target:

```console
cd /tmp/swan-dict-obs/home:lxyan3/swan-dict
osc buildlog Fedora_44 x86_64 | tail -n 30
```

Other examples:

```console
osc buildlog Debian_13 x86_64 | tail -n 30
osc buildlog Arch_Extra x86_64 | tail -n 30
```

## Trigger Rebuilds

Rebuild one target:

```console
osc rebuild home:lxyan3 swan-dict Fedora_44 x86_64
```

Rebuild all targets:

```console
osc rebuild home:lxyan3 swan-dict
```

## Release Checklist

1. Set versions consistently:
   - `CMakeLists.txt`
   - `applet/metadata.json`
   - `packaging/obs/swan-dict.spec`
   - `packaging/obs/PKGBUILD`
   - `packaging/obs/debian/changelog`
2. Regenerate patches if generated Digital Clock files changed:

```console
scripts/regenerate-patches.sh
```

3. Build locally:

```console
cmake -B build -S .
cmake --build build
```

4. Prepare and commit OBS source files.
5. Confirm these targets pass:

```text
Arch_Extra x86_64
Debian_13 x86_64
Fedora_44 x86_64
```

6. Tag after OBS is green:

```console
git tag -a v1.0.0 -m "Swan Dict 1.0.0"
git push origin main --tags
```
