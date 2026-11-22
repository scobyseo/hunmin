#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Run the IBus settings window against a build tree (issue #327,
# docs/SPEC-FRONTEND.md §FE17). The window is a separate process the IBus panel
# normally starts from its Preferences item; this is the same binary, started by
# hand, so a person can use it before the package is installed.
#
#   tools/ibus-settings-run.sh              the real store (dconf subtree
#                                           /org/freedesktop/ibus/engine/hunmin/)
#   tools/ibus-settings-run.sh --scratch    a throwaway keyfile store under a
#                                           temporary XDG_CONFIG_HOME
#
# What the window does NOT do: edit org.freedesktop.ibus.general.hotkey.trigger.
# Only the ENGINE's startup apply reconciles that list (store/store.c), so a
# window session never changes how IBus switches input methods. Running the
# engine — or `hunmin-ibus-settings --print` — does; that is the shipped
# behaviour and it is why the test lanes use GSETTINGS_BACKEND=memory.
#
# To read the rows back after changing them (no apply, no trigger edit):
#   GSETTINGS_SCHEMA_DIR=<build>/frontends/ibus/glib-2.0/schemas \
#     gsettings list-recursively org.freedesktop.ibus.engine.hunmin
# To throw the store away again:
#   dconf reset -f /org/freedesktop/ibus/engine/hunmin/
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${HUNMIN_BUILD_DIR:-$here/build}
bin=$build/frontends/ibus/hunmin-ibus-settings
schemas=$build/frontends/ibus/glib-2.0/schemas

[ -x "$bin" ] || {
    echo "not built: $bin" >&2
    echo "  cmake -B build && cmake --build build    (needs gtk+-3.0; see docs/ENVIRONMENT.md)" >&2
    exit 1
}
[ -f "$schemas/gschemas.compiled" ] || {
    echo "schema not compiled: $schemas" >&2
    echo "  glib-compile-schemas is required; the build prints a warning when it is missing" >&2
    exit 1
}

export GSETTINGS_SCHEMA_DIR=$schemas
export HUNMIN_DATA_PATH=${HUNMIN_DATA_PATH:-$here/libhunmin/data}

case "${1:-}" in
    --scratch)
        scratch=$(mktemp -d "${TMPDIR:-/tmp}/hunmin-settings-XXXXXX")
        export GSETTINGS_BACKEND=keyfile XDG_CONFIG_HOME=$scratch
        echo "store: scratch ($scratch) — nothing persists"
        ;;
    ""|--real)
        echo "store: dconf, /org/freedesktop/ibus/engine/hunmin/"
        echo "       reset with: dconf reset -f /org/freedesktop/ibus/engine/hunmin/"
        ;;
    *)
        echo "usage: $0 [--real|--scratch]" >&2
        exit 2
        ;;
esac

if [ -z "${WAYLAND_DISPLAY:-}" ] && [ -z "${DISPLAY:-}" ]; then
    echo "no display: set DISPLAY or WAYLAND_DISPLAY, or run under xvfb-run" >&2
    exit 1
fi

exec "$bin"