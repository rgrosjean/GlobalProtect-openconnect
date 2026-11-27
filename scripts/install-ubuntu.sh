#!/bin/bash -e

sudo apt-get update

# Newer releases (e.g. Ubuntu 26.04) renamed libqt6websockets6-dev to qt6-websockets-dev
if apt-cache show qt6-websockets-dev >/dev/null 2>&1; then
    QT_WEBSOCKETS_PKG=qt6-websockets-dev
else
    QT_WEBSOCKETS_PKG=libqt6websockets6-dev
fi

sudo apt-get install -y \
    build-essential \
    qt6-base-dev \
    "$QT_WEBSOCKETS_PKG" \
    qt6-webengine-dev \
    qtkeychain-qt6-dev \
    openconnect

./scripts/install.sh
