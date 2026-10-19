#!/usr/bin/env bash
set -euo pipefail
bash ./package.sh
mkdir -p dist
cp build/Swarm.pkg dist/installer.pkg
pkgutil --check-signature dist/installer.pkg
xcrun stapler validate dist/installer.pkg
