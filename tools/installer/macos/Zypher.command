#!/bin/bash
# Zypher launcher. Double-click in Finder to open the editor, or pass runtime
# options to the command line runtime, for example:  ./Zypher.command --help
set -u

APP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EDITOR="$APP/Zypher Editor.app"
RUNTIME="$APP/bin/zypher_runtime"

hold() {
    echo
    read -r -p "Press Return to close this window." _
}

if [ "$#" -gt 0 ]; then
    if [ ! -x "$RUNTIME" ]; then
        echo "Zypher could not start: bin/zypher_runtime is missing."
        hold
        exit 1
    fi
    "$RUNTIME" "$@"
    status=$?
    [ -t 0 ] && hold
    exit "$status"
fi

if [ -d "$EDITOR" ]; then
    open "$EDITOR"
    exit 0
fi

if [ ! -x "$RUNTIME" ]; then
    echo "Zypher could not start: neither 'Zypher Editor.app' nor bin/zypher_runtime is present."
    echo "Download a fresh copy from https://github.com/itriedcoding/Zypher/releases"
    hold
    exit 1
fi

OUT="${HOME}/Zypher/artifacts"
SCENE="$APP/share/zypher/samples/demo/scene.zscene"
PACK="$APP/share/zypher/samples/demo/dist/demo.zpkg"

echo
"$RUNTIME" --version
echo
echo "The editor is not part of this build; rendering the demo scene instead."
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
hold
