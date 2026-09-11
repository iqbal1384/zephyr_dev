#!/usr/bin/env bash
# Copies these full source files on top of a freshly `west update`-d
# zephyr/ checkout. Run after every `west update`. Paths here mirror
# their location under zephyr/ exactly, so this is a straight copy,
# not a patch -- if upstream changed the same files, diff before
# overwriting (see README.md at the repo root for the workflow).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
zephyr_dir="$script_dir/../../../../zephyr"

cd "$script_dir"
find . -type f ! -name 'apply.sh' | while read -r f; do
    rel="${f#./}"
    echo "Copying $rel..."
    mkdir -p "$zephyr_dir/$(dirname "$rel")"
    cp "$rel" "$zephyr_dir/$rel"
done
