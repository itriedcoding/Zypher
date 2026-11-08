#!/usr/bin/env bash
# Tag a release and push it. The Release workflow builds macOS and Windows
# archives and attaches them to the GitHub Release.
#
# Usage: ./tools/scripts/release.sh 0.1.0 ["release notes"]
set -euo pipefail

version="${1:-}"
notes="${2:-Zypher release}"

if [[ -z "$version" ]]; then
    echo "usage: $0 <version> [notes]  (e.g. $0 0.1.0)" >&2
    exit 2
fi

project_version="$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
if [[ "$project_version" != "$version" ]]; then
    echo "error: CMakeLists.txt declares $project_version, but $version was requested" >&2
    echo "       bump project(Zypher VERSION ...) first, or pass $project_version" >&2
    exit 1
fi

tag="v${version}"
if git rev-parse "$tag" >/dev/null 2>&1; then
    echo "error: tag $tag already exists locally" >&2
    exit 1
fi

git tag -a "$tag" -m "Zypher ${version} - ${notes}"
git push origin "HEAD:main"
git push origin "$tag"
echo "pushed $tag; the Release workflow will publish the archives"
