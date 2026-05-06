#!/bin/bash
# port/wx/build-shell.sh — cross-compile the wxWidgets shell for SPARC.
#
# Runs inside the sst-build-pipeline sparc-userland-build container.
# Uses prepare_sysroot + ensure_staged from common.sh to lay down
# wxWidgets and its runtime closure, then drives sparc-sun-solaris2.7-g++
# via the staged wx-config script.
#
# Output: ${OUTPUT}/mushclient (a SPARC Solaris 7 ELF).
#
# Invoke from sst-build-pipeline repo root:
#
#   docker compose \
#     -f cross-build-userland/docker-compose.yml \
#     -f local-dev/compose.override.yml \
#     run --rm --entrypoint /bin/bash \
#     -v /path/to/mushclient:/opt/mushclient:ro \
#     userland-build \
#     /opt/mushclient/port/wx/build-shell.sh

set -euo pipefail

PACKAGES_DIR="/opt/cross-build-userland/packages"
# shellcheck source=/dev/null
source /opt/cross-build-userland/common.sh

prepare_sysroot

ensure_staged \
    libsolcompat libgcc libstdcxx zlib libpng libjpeg expat \
    libiconv libffi gettext libxrender pixman fribidi freetype2 \
    fontconfig glib libxft harfbuzz cairo pango wxwidgets

# Stage wxWidgets's bin/wx-config — the symlink in the staging tarball
# dangles (target = /opt/sst/lib/wx/config/<tuple>); we invoke the
# real config script directly.
# Glob for whichever toolkit was actually staged (x11univ vs motif vs ...)
# so swapping the wxwidgets package's --with-x11 / --with-motif config
# doesn't require a code change here.
WX_CONFIG_REAL="$(ls "${SYSROOT}${PREFIX}/lib/wx/config/sparc-sun-solaris2.7-"*-unicode-static-3.0 2>/dev/null | head -1)"
[ -n "${WX_CONFIG_REAL}" ] && [ -x "${WX_CONFIG_REAL}" ] \
    || { echo "wx-config target missing under ${SYSROOT}${PREFIX}/lib/wx/config/"; exit 1; }
echo "+ wx-config: ${WX_CONFIG_REAL##*/}"

# Strip pkg-config-injected sysroot prefix once and re-add ours, so
# -I and -L paths land inside the staged sysroot exactly once.
# Also strip wx-config's -R/<sysroot> entries entirely — those would
# bake build-host paths into DT_RPATH. We supply runtime RPATH below.
fix_paths() {
    sed "s|${SYSROOT}/opt/sst/|/opt/sst/|g; s|/opt/sst/|${SYSROOT}/opt/sst/|g; s| -R${SYSROOT}[^ ]*||g"
}

CXXFLAGS="$("${WX_CONFIG_REAL}" --cxxflags | fix_paths) --sysroot=${SYSROOT}"
LIBS="$("${WX_CONFIG_REAL}" --libs | fix_paths)"

# Solaris 7 thread support is libpthread.so.1 (provides _getfp +
# pthread_*). cross-gcc 15's `-pthreads` (Solaris cc convention)
# doesn't expand into -lpthread; pass it explicitly.
LIBS="${LIBS} -lpthread"

# Modern GNU ld defaults to DT_RUNPATH; Solaris 7 ld.so only honours
# DT_RPATH. Flip back to old dtags so the runtime linker actually
# finds /opt/sst/lib + /usr/openwin/lib at startup.
LIBS="${LIBS} -Wl,--disable-new-dtags"

# Bake the runtime RPATH at link time. /usr/dt/lib is included so
# libXm.so.4 (Solaris 7 CDE Motif runtime) resolves at startup when
# wxWidgets was built --with-motif. Setting RPATH at link time avoids
# patchelf's post-link surgery, which on Solaris 7 inserts an extra
# RW LOAD segment at virtual address 0 that the kernel silently
# SIGKILLs the process for trying to map (NULL page).
LIBS="${LIBS} -Wl,-rpath,/opt/sst/lib:/usr/openwin/lib:/usr/dt/lib"

# Solaris 7 ld.so.1 calls DT_INIT but not DT_INIT_ARRAY. Point DT_INIT
# at our INIT_ARRAY walker so all the C++ ctors / pango+glib
# `__attribute__((constructor))` functions actually run before main.
# wxMotif builds DON'T pull in GObject/glib/pango — those constructors
# don't exist in this toolkit, and the walker on its own still wants
# to drive `__init_array_start..__init_array_end` for the main exe's
# C++ ctors (wxFrame / wxApp / our static AnsiTelnetParser).
LIBS="${LIBS} -Wl,-init,sst_sol7_run_init_array"

WX_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${WX_DIR}"

OBJ_OUT="/opt/output/mushclient-shell.o"
BIN_OUT="/opt/output/mushclient"

echo "+ ${CXX} ${CXXFLAGS} -c main.cpp -o ${OBJ_OUT}"
# shellcheck disable=SC2086
${CXX} ${CXXFLAGS} -c main.cpp -o "${OBJ_OUT}"

echo "+ ${CXX} ${OBJ_OUT} ${LIBS} -o ${BIN_OUT}"
# shellcheck disable=SC2086
${CXX} "${OBJ_OUT}" ${LIBS} -o "${BIN_OUT}"

# NOTE: do NOT run `patchelf --set-rpath` here. On Solaris 7, patchelf's
# post-link surgery rewrites program headers in a way that inserts an
# extra RW LOAD segment at virtual address 0. The kernel exec(2) silently
# SIGKILLs any process whose binary requests a mapping at the NULL page.
# RPATH is set at link time via -Wl,-rpath above, which is enough.

echo ""
file "${BIN_OUT}" 2>/dev/null || head -c 20 "${BIN_OUT}" | od -An -c
ls -lh "${BIN_OUT}"
