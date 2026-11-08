#!/bin/bash
# Zypher launcher. Double-click in Finder to render the demo, or run it from a
# terminal with runtime options, for example:  ./Zypher.command --help
set -u

APP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME="$APP/bin/zypher_runtime"

hold() {
    echo
    read -r -p "Press Return to close this window." _
}

if [ ! -x "$RUNTIME" ]; then
    echo "Zypher could not start: bin/zypher_runtime is missing."
    echo "Download a fresh copy from https://github.com/itriedcoding/Zypher/releases"
    hold
    exit 1
fi

if [ "$#" -gt 0 ]; then
    "$RUNTIME" "$@"
    status=$?
    [ -t 0 ] && hold
    exit "$status"
fi

OUT="${HOME}/Zypher/artifacts"
SCENE="$APP/share/zypher/samples/demo/scene.zscene"
PACK="$APP/share/zypher/samples/demo/dist/demo.zpkg"

echo
"$RUNTIME" --version
echo
echo "Rendering the demo scene (1280x720, 2 frames, 120 simulation steps)..."
echo

args=(--output "$OUT" --width 1280 --height 720 --frames 2 --steps 120 --dump-frame)
if [ -f "$PACK" ]; then
    args=(--pack "$PACK" "${args[@]}")
fi
if [ -f "$SCENE" ]; then
    args=(--scene "$SCENE" "${args[@]}")
fi

"$RUNTIME" "${args[@]}"
status=$?

if [ "$status" -ne 0 ]; then
    echo
    echo "Zypher exited with code ${status}. See ${OUT}/zypher.log for details."
    hold
    exit "$status"
fi

echo
echo "Frame and report written to:"
echo "  ${OUT}/frame.png"
echo "  ${OUT}/report.json"
echo
echo "Next steps:"
echo "  ./Zypher.command --help"
echo "  ./Zypher.command --list-assets --pack \"${PACK}\""
echo "  ./Zypher.command --scene <scene> --pack <pack> --output <dir>"
hold
