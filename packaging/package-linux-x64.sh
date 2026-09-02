#!/usr/bin/env bash
set -euo pipefail

repository_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$repository_dir/cmake-build-release-wsl-linux-x86_64"}
[[ "$build_dir" = /* ]] || build_dir="$repository_dir/$build_dir"
case "$build_dir" in
  "$repository_dir"/cmake-build-*) ;;
  *) echo "Build directory must be cmake-build-* inside the repository." >&2; exit 2 ;;
esac

cmake -S "$repository_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/home/akbailey/Qt/6.10.3/gcc_64 \
  -DOPTIME_DEPLOY_QT_RUNTIME=OFF
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

qt_dir=${OPTIME_QT_DIR:-/home/akbailey/Qt/6.10.3/gcc_64}
linuxdeploy=${LINUXDEPLOY_EXECUTABLE:-"$qt_dir/bin/linuxdeploy"}
linuxdeploy_qt_plugin=${LINUXDEPLOY_QT_PLUGIN:-"$qt_dir/bin/linuxdeploy-plugin-qt"}
qmake=${LINUXDEPLOY_QMAKE:-"$qt_dir/bin/qmake"}

for required_tool in "$linuxdeploy" "$linuxdeploy_qt_plugin" "$qmake"; do
  [[ -x "$required_tool" ]] || {
    echo "Required AppImage tool is missing or not executable: $required_tool" >&2
    exit 3
  }
done

app_dir="$build_dir/OpTimeNTP.AppDir"
appimage_path="$build_dir/OpTimeNTP.AppImage"
plugin_link="$build_dir/linuxdeploy-plugin-qt"
plugin_filter_dir="$build_dir/qt-plugins-for-appimage"
qmake_wrapper="$build_dir/qmake-for-linuxdeploy"

rm -rf -- "$app_dir"
rm -rf -- "$plugin_filter_dir"
rm -f -- "$appimage_path" "$plugin_link" "$qmake_wrapper"
cmake --install "$build_dir" --prefix "$app_dir/usr"
ln -s -- "$linuxdeploy_qt_plugin" "$plugin_link"

mkdir -p -- "$plugin_filter_dir/imageformats"
for plugin_entry in "$qt_dir/plugins"/*; do
  [[ -e "$plugin_entry" ]] || continue
  plugin_name=${plugin_entry##*/}
  if [[ "$plugin_name" == imageformats ]]; then
    for image_plugin in "$plugin_entry"/*.so*; do
      [[ -e "$image_plugin" ]] || continue
      image_plugin_name=${image_plugin##*/}
      [[ "$image_plugin_name" == libqtiff.so* ]] && continue
      ln -s -- "$image_plugin" "$plugin_filter_dir/imageformats/$image_plugin_name"
    done
  else
    ln -s -- "$plugin_entry" "$plugin_filter_dir/$plugin_name"
  fi
done

cat > "$qmake_wrapper" <<'EOF'
#!/usr/bin/env sh
set -eu

if [ "$1" = "-query" ] && [ "${2:-}" = "QT_INSTALL_PLUGINS" ]; then
  printf '%s\n' "$LINUXDEPLOY_FILTERED_QT_PLUGINS"
  exit 0
fi
if [ "$1" = "-query" ] && [ "$#" -eq 1 ]; then
  "$LINUXDEPLOY_REAL_QMAKE" "$@" |
    sed "s|^QT_INSTALL_PLUGINS:.*$|QT_INSTALL_PLUGINS:$LINUXDEPLOY_FILTERED_QT_PLUGINS|"
  exit $?
fi
exec "$LINUXDEPLOY_REAL_QMAKE" "$@"
EOF
chmod 755 "$qmake_wrapper"

wayland_platform_plugin="$qt_dir/plugins/platforms/libqwayland.so"
[[ -f "$wayland_platform_plugin" ]] || {
  echo "Required Qt Wayland platform plugin is missing: $wayland_platform_plugin" >&2
  exit 4
}
mkdir -p -- "$app_dir/usr/plugins/platforms"
cp -L -- "$wayland_platform_plugin" "$app_dir/usr/plugins/platforms/"

for wayland_plugin_dir in \
  wayland-decoration-client \
  wayland-graphics-integration-client \
  wayland-shell-integration; do
  source_plugin_dir="$qt_dir/plugins/$wayland_plugin_dir"
  [[ -d "$source_plugin_dir" ]] || continue
  mkdir -p -- "$app_dir/usr/plugins/$wayland_plugin_dir"
  cp -L -- "$source_plugin_dir"/*.so "$app_dir/usr/plugins/$wayland_plugin_dir/"
done

for qt_runtime_library in libQt6WaylandClient.so.6 libQt6OpenGL.so.6; do
  source_library="$qt_dir/lib/$qt_runtime_library"
  [[ -e "$source_library" ]] || continue
  mkdir -p -- "$app_dir/usr/lib"
  cp -L -- "$source_library" "$app_dir/usr/lib/$qt_runtime_library"
done

PATH="$build_dir:$qt_dir/bin:$PATH" \
LD_LIBRARY_PATH="$qt_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LINUXDEPLOY_REAL_QMAKE="$qmake" \
LINUXDEPLOY_FILTERED_QT_PLUGINS="$plugin_filter_dir" \
QMAKE="$qmake_wrapper" \
ARCH=x86_64 \
LDAI_OUTPUT="$appimage_path" \
"$linuxdeploy" \
  --appdir "$app_dir" \
  --executable "$app_dir/usr/bin/OpTimeNTP" \
  --desktop-file "$app_dir/usr/share/applications/io.github.opmatrixsoftware.optimentp.desktop" \
  --icon-file "$app_dir/usr/share/icons/hicolor/scalable/apps/io.github.opmatrixsoftware.optimentp.svg" \
  --exclude-library 'libglib-2.0.so*' \
  --exclude-library 'libgthread-2.0.so*' \
  --plugin qt \
  --output appimage

[[ -x "$appimage_path" ]] || {
  echo "AppImage was not created: $appimage_path" >&2
  exit 4
}

echo "Created $appimage_path"
