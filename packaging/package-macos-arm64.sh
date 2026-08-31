#!/usr/bin/env bash
set -euo pipefail

repository_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$repository_dir/cmake-build-macos-arm64-release"}
[[ "$build_dir" = /* ]] || build_dir="$repository_dir/$build_dir"
case "$build_dir" in
  "$repository_dir"/cmake-build-*) ;;
  *) echo "Build directory must be cmake-build-* inside the repository." >&2; exit 2 ;;
esac

cmake -S "$repository_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH=/Library/Qt/6.10.3/macos \
  -DOPTIME_DEPLOY_QT_RUNTIME=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

stage_dir="$build_dir/package/OpTimeNTP"
rm -rf -- "$stage_dir"
cmake --install "$build_dir" --prefix "$stage_dir"

archive_path="$build_dir/OpTimeNTP_macos_arm64.zip"
rm -f -- "$archive_path"
ditto -c -k --sequesterRsrc --keepParent "$stage_dir" "$archive_path"
echo "Created $archive_path"
