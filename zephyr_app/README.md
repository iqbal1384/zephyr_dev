# zephyr_app

Application repo for the Curie 4.0 / STM32 board work, pushed to GitHub
as **`zephyr_dev`** (`git@github.com:zafar4472/zephyr_dev.git`) — the
local folder is still named `zephyr_app`, only the remote's name
changed. This repo is the **west manifest repository**: cloning it and
running `west update` pulls in `zephyr` straight from upstream
(`zephyrproject-rtos/zephyr`, no fork), plus all of zephyr's own
modules (HAL, mcuboot, etc.) as sibling directories — same layout as
the current dev workspace. `zephyr_app` owns the **full source** of its
local STM32MP2 changes to zephyr directly
(`stm32/mp2_m33/zephyr_changes/`) — real files, not diffs — copied back
onto `zephyr/` after `west update`.

```
zephyr_dev/
  zephyr_app/   <- this repo (west topdir's manifest repo)
    west.yml                          <- pins zephyr's revision, imports its own manifest
    stm32/mp2_m33/zephyr_changes/     <- your STM32MP2 files, path-mirrored to zephyr/, + apply.sh
  zephyr/       <- pulled by `west update`, straight from zephyrproject-rtos
  bootloader/   <- pulled by `west update` (mcuboot)
  modules/      <- pulled by `west update` (hal, crypto, fs, ...)
```

## Why this restructuring

The old `zephyr_app` remote (`git@github.com:zafar4472/zephyr_app.git`)
started returning `ERROR: Repository not found` on push, even though the
SSH key authenticates correctly as `zafar4472` (confirmed with
`ssh -T git@github.com`). Since `origin/master` was previously synced to
a real commit, the repo was likely deleted, renamed, or transferred on
GitHub. Rather than chase that, we started fresh with a new repo,
`zephyr_dev`.

Separately, the `zephyr/` checkout in the workspace was a plain clone of
`zephyrproject-rtos/zephyr` (10000+ commits behind upstream `main`) with
several **uncommitted local patches** (STM32MP2 clock control, DTS
bindings, blinky sample), tracked by nothing. Since you want to keep
pulling fresh upstream features into `zephyr/` yourself over time
without maintaining a personal fork, and you want to own the full code
directly rather than opaque diffs:

- `zephyr/` is a plain, unmodified clone of `zephyrproject-rtos/zephyr`
  — single `origin` remote, no fork.
- `zephyr_app/stm32/mp2_m33/zephyr_changes/` has the 7 changed/added
  files as **full files**, at the same relative path they live at
  inside `zephyr/` (e.g.
  `zephyr_changes/drivers/clock_control/clock_stm32_ll_mp2.c` mirrors
  `zephyr/drivers/clock_control/clock_stm32_ll_mp2.c`). You can open,
  edit, and diff these directly like any other file in the repo.
- `zephyr_app/stm32/mp2_m33/zephyr_changes/apply.sh` copies them onto
  `zephyr/` after a `west update`.
- `west.yml` pins `zephyr` to a specific commit
  (`3c36361caba41cb7fdf31b8b8ddae00f74fa0b62`) rather than a floating
  branch, since these files don't move with upstream on their own —
  bump the pin deliberately when you want newer Zephyr.

## Pulling new upstream Zephyr features later

```bash
# 1. bump the revision in zephyr_app/west.yml to the commit/tag you want
cd zephyr_dev
west update                                    # fetches that revision into zephyr/
zephyr_app/stm32/mp2_m33/zephyr_changes/apply.sh   # copies your files back onto zephyr/
```
If upstream changed one of the same 7 files in the meantime, diff it
against `zephyr_changes/` before running `apply.sh` (or after, using
`git -C zephyr diff` before it's overwritten) and fold the upstream
changes into your copy by hand — there's no automatic merge with a
plain file copy, unlike a patch or a fork branch.

## What's already done (local, nothing pushed yet)

- [x] `zephyr/` reset to a plain, unmodified `zephyrproject-rtos/zephyr`
      clone at `3c36361c` (single `origin` remote, no fork)
- [x] STM32MP2 changes copied in full to
      `zephyr_app/stm32/mp2_m33/zephyr_changes/` (path-mirrored to
      `zephyr/`), originals left in place in `zephyr/`'s working tree so
      your local checkout is unchanged day-to-day
- [x] `zephyr_app/stm32/mp2_m33/zephyr_changes/apply.sh` added
- [x] `zephyr_app/west.yml` drafted (T2 manifest topology, pins zephyr
      to `3c36361c`, imports zephyr's own manifest so
      modules/mcuboot/etc. still resolve normally)
- [x] `zephyr_dev` repo created on GitHub, local `zephyr_app/` remote
      already points at it (`git remote set-url origin
      git@github.com:zafar4472/zephyr_dev.git`)
- [ ] `zephyr_app` committed and pushed
- [ ] `.west/config` switched to the new manifest

## Steps to finish (do these in order)

### 1. Commit and push `zephyr_app` (to the `zephyr_dev` remote)

There are already some other uncommitted changes in this repo
(`stm32/curie4.0/documentation/pins.md`, `stm32/documentation/build.md`,
plus untracked `.vscode/` and `stm32/curie4.0/adc_voltage/`). Review and
commit what you want kept, then:

```bash
cd zephyr_dev/zephyr_app
git add west.yml README.md stm32/mp2_m33/zephyr_changes/   # plus whatever else you want in this commit
git commit -m "Add west manifest and STM32MP2 zephyr changes for app-as-manifest topology"

git branch -M main   # match zephyr_dev's default branch name
git push -u origin main --force
```

The local `origin` remote already points at `zephyr_dev` (done). The
`--force` is needed because `zephyr_dev` currently has one throwaway
commit (`first commit`, just `# zephyr_dev` in a README) from creating
the repo on GitHub, with no shared history with your local commits —
safe to overwrite since it has no real content. If you'd rather keep
that commit, use `git pull origin main --allow-unrelated-histories`
before pushing instead of `--force`.

### 2. Point west at the new manifest

Currently `zephyr_dev/.west/config` still points at `zephyr` as the
manifest (the old T1 topology). Switch it to `zephyr_app`:

```bash
cd zephyr_dev
west config manifest.path zephyr_app
west config manifest.file west.yml
```

This edits `.west/config`'s `[manifest]` section to:
```ini
[manifest]
path = zephyr_app
file = west.yml
group-filter = +bootloader,+crypto
```
(`group-filter` is unchanged — it's a local workspace setting, not tied
to which repo is the manifest.)

### 3. Verify

```bash
cd zephyr_dev
west manifest --validate
west list          # should show zephyr, mcuboot, hal modules, etc.
west update         # should be a no-op / fast-forward, since zephyr/ is
                     # already checked out at the pinned revision
git -C zephyr status --short   # should show the same 7 changed files as before
```

### 4. Confirm a fresh clone works (optional but recommended)

In a scratch directory:
```bash
west init -m git@github.com:zafar4472/zephyr_dev.git --mr main fresh_test
cd fresh_test
west update
zephyr_app/stm32/mp2_m33/zephyr_changes/apply.sh
```
You should end up with `zephyr_app/` and `zephyr/` as siblings, with
`zephyr/` carrying the STM32MP2 changes, matching the current workspace.

### 5. Clean up

A backup directory was created while iterating on this — safe to remove
once steps 1–4 are confirmed working:
```bash
rm -rf zephyr_dev/zephyr_git_vendor_squashed_backup
```
(an earlier attempt that squashed zephyr's history away — superseded)
