#!/bin/bash

OUTPUT=$1

VERSION_MAJOR="0"
VERSION_MINOR="2"
VERSION_PATCH="0"
GIT_REVISION="$(git rev-parse --short HEAD)"
DATE="$(date)"

cat << EOF > "$OUTPUT"
/* This file was generated automatically */

#pragma once
const int NUSAOS_VERSION_MAJOR = ${VERSION_MAJOR};
const int NUSAOS_VERSION_MINOR = ${VERSION_MINOR};
const int NUSAOS_VERSION_PATCH = ${VERSION_PATCH};
const char* NUSAOS_VERSION_STRING = "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}";
const char* NUSAOS_REVISION = "${GIT_REVISION}";
EOF