#!/bin/sh

SCRIPT_DIR=$(dirname "$0")
INSTALL_DIR=$(cd "$SCRIPT_DIR" 2>/dev/null && pwd -P)

__execute_para() {
    if [ -n "${PARA_CORE:-}" ]; then
      exec "$PARA_CORE"/bin/EM "$@"
    else
      echo "Error: PARA_CORE is not set. Run 'source para-env-setup.sh' from the PARA SDK." >&2
      exit 1
    fi
}

# configure environment (edit below if needed)
export PARA_CONF="$INSTALL_DIR/etc"
export PARA_DATA="$INSTALL_DIR/var"
export PARA_APPL="$INSTALL_DIR/opt"

__execute_para "$@"
        