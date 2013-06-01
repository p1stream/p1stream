#!/bin/sh

set -e -x

ORCC="${BUILT_PRODUCTS_DIR}/orcc"

${ORCC} $@ --header         -o "${DERIVED_FILE_DIR}/${INPUT_FILE_BASE}.h" "${INPUT_FILE_DIR}/${INPUT_FILE_NAME}"
${ORCC} $@ --implementation -o "${DERIVED_FILE_DIR}/${INPUT_FILE_BASE}.c" "${INPUT_FILE_DIR}/${INPUT_FILE_NAME}"
