#!/bin/sh

set -e -x

YASM="${BUILT_PRODUCTS_DIR}/yasm"

${YASM} $@ -o "${DERIVED_FILE_DIR}/${INPUT_FILE_BASE}.o" "${INPUT_FILE_DIR}/${INPUT_FILE_NAME}"
