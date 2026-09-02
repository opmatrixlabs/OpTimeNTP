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

package_dir="$build_dir/package"
rm -rf -- "$package_dir"
cmake --install "$build_dir" --prefix "$package_dir"

app_path="$package_dir/OpTimeNTP.app"
[[ -d "$app_path/Contents/MacOS" ]] || {
  echo "Application bundle was not created: $app_path" >&2
  exit 3
}

echo "Created $app_path"
