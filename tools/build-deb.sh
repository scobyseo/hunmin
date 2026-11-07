#!/usr/bin/env bash
# build-deb.sh — one command for "build the release .deb from wherever I am".
#
# It picks the route the host allows, in this order, and says which one it
# took (a build whose route is invisible is a build nobody can reproduce):
#
#   HUNMIN_LINUX_HOST set        -> tools/build-deb-linux.sh  (the LAN box)
#   Git Bash / MSYS + wsl.exe    -> tools/build-deb-wsl.sh    (the WSL guest)
#   anything else (native Linux) -> tools/build-release.sh deb
#
#   tools/build-deb.sh                     # auto
#   tools/build-deb.sh --route=linux       # force the box (fails if unset)
#   tools/build-deb.sh --keep-src          # leave the remote/copy tree behind
#
# Why the box first: the deferred `docs/` plan (PLAN-RELEASE-ROUTINE.md) makes
# a native Linux host the long-term Linux side. Until it is wired into CI this
# is the switch that uses it, and unsetting HUNMIN_LINUX_HOST is the rollback.
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tools/host-env.sh
. "$HERE/host-env.sh"
hunmin_load_env "$HOME/.hunmin-env" "$(dirname "$HERE")/.env"

ROUTE=auto
KEEP=0
for arg in "$@"; do
    case "$arg" in
        --route=*) ROUTE=${arg#--route=} ;;
        --keep-src) KEEP=1 ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "build-deb: unknown argument '$arg'" >&2; exit 2 ;;
    esac
done

extra=()
[ "$KEEP" = 1 ] && extra+=(--keep-src)

run_linux() {
    [ -n "$(hunmin_linux_host)" ] \
        || { echo "build-deb: --route=linux needs HUNMIN_LINUX_HOST" >&2; exit 2; }
    echo "=== route: linux box ($HUNMIN_LINUX_HOST)"
    exec "$HERE/build-deb-linux.sh" "${extra[@]}"
}

run_wsl() {
    local script
    # wsl.exe needs the guest's view of the path; the guest cannot read C:/…
    script=$(hunmin_wsl_path "$HERE/build-deb-wsl.sh")
    echo "=== route: WSL guest ($script)"
    exec wsl.exe bash "$script" "${extra[@]}"
}

case "$ROUTE" in
    auto)
        if [ -n "$(hunmin_linux_host)" ]; then
            run_linux
        elif hunmin_is_msys && command -v wsl.exe >/dev/null 2>&1; then
            run_wsl
        else
            echo "=== route: native dpkg-buildpackage"
            exec "$HERE/build-release.sh" deb
        fi
        ;;
    linux)  run_linux ;;
    wsl)    run_wsl ;;
    native) echo "=== route: native dpkg-buildpackage"
            exec "$HERE/build-release.sh" deb ;;
    *) echo "build-deb: unknown route '$ROUTE' (auto|linux|wsl|native)" >&2; exit 2 ;;
esac