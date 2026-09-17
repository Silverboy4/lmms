#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${LMMS_SYNTHETIC_BUILD_DIR:-${SOURCE_DIR}/build-synthetic-zay}"
INSTALL_DIR="${LMMS_SYNTHETIC_INSTALL_DIR:-${SOURCE_DIR}/target-synthetic-zay}"
BUILD_JOBS="${LMMS_SYNTHETIC_BUILD_JOBS:-$(nproc)}"

usage() {
	cat <<'EOF'
Synthetic Zay LMMS build helper for Fedora

Usage:
  buildtools/build-fedora-synthetic-zay.sh deps
  buildtools/build-fedora-synthetic-zay.sh configure
  buildtools/build-fedora-synthetic-zay.sh build
  buildtools/build-fedora-synthetic-zay.sh test
  buildtools/build-fedora-synthetic-zay.sh run
  buildtools/build-fedora-synthetic-zay.sh install-user
  buildtools/build-fedora-synthetic-zay.sh package
  buildtools/build-fedora-synthetic-zay.sh all

Only "deps" installs system packages and uses sudo. The custom build and
installation stay inside this repository, alongside the regular Fedora LMMS.
EOF
}

install_dependencies() {
	if ! command -v dnf >/dev/null 2>&1; then
		echo "This dependency helper is intended for Fedora systems with dnf." >&2
		exit 1
	fi

	# DNF5 and legacy DNF package their builddep command separately.
	sudo dnf install -y dnf5-plugins || sudo dnf install -y dnf-plugins-core
	sudo dnf install -y \
		git cmake ninja-build ccache gcc-c++ \
		qt5-qtbase-devel qt5-qtbase-private-devel qt5-qtsvg-devel \
		qt5-qtx11extras-devel

	# Use Fedora's LMMS source package as the version-aware dependency list.
	# DNF enables the matching source repository for builddep as needed.
	sudo dnf builddep -y lmms
}

configure_build() {
	git -C "${SOURCE_DIR}" submodule update --init --recursive

	cmake \
		-S "${SOURCE_DIR}" \
		-B "${BUILD_DIR}" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
		-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
		-DFORCE_VERSION=1.3.0-alpha-synthetic-zay
}

build_lmms() {
	cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"
}

test_lmms() {
	ctest --test-dir "${BUILD_DIR}" \
		--output-on-failure \
		--parallel "${BUILD_JOBS}"
}

run_lmms() {
	if [[ ! -x "${BUILD_DIR}/lmms" ]]; then
		echo "No custom LMMS executable found. Run 'configure' and 'build' first." >&2
		exit 1
	fi
	"${BUILD_DIR}/lmms"
}

install_user() {
	cmake --install "${BUILD_DIR}"
	echo "Custom LMMS installed below: ${INSTALL_DIR}"
}

package_lmms() {
	cmake --build "${BUILD_DIR}" --target package --parallel "${BUILD_JOBS}"
	find "${BUILD_DIR}" -maxdepth 1 -type f -name 'lmms-*.AppImage' -print
}

case "${1:-help}" in
	deps)
		install_dependencies
		;;
	configure)
		configure_build
		;;
	build)
		build_lmms
		;;
	test)
		test_lmms
		;;
	run)
		run_lmms
		;;
	install-user)
		install_user
		;;
	package)
		package_lmms
		;;
	all)
		configure_build
		build_lmms
		test_lmms
		;;
	help|-h|--help)
		usage
		;;
	*)
		echo "Unknown command: $1" >&2
		usage >&2
		exit 2
		;;
esac
