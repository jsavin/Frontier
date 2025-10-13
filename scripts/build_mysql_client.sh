#!/usr/bin/env bash
set -euo pipefail

VERSION="3.3.5"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${MYSQL_CLIENT_PREFIX:-${ROOT_DIR}/Common/MySQL}"
ARCHES=${CMAKE_OSX_ARCHITECTURES:-"arm64;x86_64"}

if ! command -v cmake >/dev/null; then
  echo "error: cmake is required to build the MySQL client" >&2
  exit 1
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

SOURCE_URL="https://github.com/mariadb-corporation/mariadb-connector-c/archive/refs/tags/v${VERSION}.tar.gz"

mkdir -p "${PREFIX}"

pushd "$WORKDIR" >/dev/null
curl -L "$SOURCE_URL" -o mariadb-connector-c.tar.gz
mkdir src
tar -xzf mariadb-connector-c.tar.gz -C src --strip-components=1
mkdir build
cd build

cmake ../src \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DWITH_UNIT_TESTS=OFF \
  -DCMAKE_OSX_ARCHITECTURES="${ARCHES}" \
  -DMARIADB_LINK_DYNAMIC=OFF \
  -DWITH_SSL=OFF

cmake --build . --config Release --target install

# Provide libmysqlclient compatibility
if [ -f "${PREFIX}/lib/libmariadb.a" ] && [ ! -f "${PREFIX}/lib/libmysqlclient.a" ]; then
  ln -sf libmariadb.a "${PREFIX}/lib/libmysqlclient.a"
fi

if [ -d "${PREFIX}/include/mariadb" ] && [ ! -d "${PREFIX}/include/mysql" ]; then
  ln -s mariadb "${PREFIX}/include/mysql"
fi

popd >/dev/null

echo "MySQL (MariaDB) client libraries installed under ${PREFIX}" 
