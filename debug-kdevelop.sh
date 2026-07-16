#!/bin/bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

unset KDEV_DBUS_ID
unset APPLICATION
unset KDEV_ATTACHED_PID

export QT_PLUGIN_PATH="$DIR/install/lib/x86_64-linux-gnu/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

exec /usr/bin/kdevelop -s craft-platforms "$@"
