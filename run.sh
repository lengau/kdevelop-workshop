#!/bin/bash

# Determine the absolute path of the directory containing this script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure the build is up to date and installed locally
echo "Building and installing the plugin..."
(cd "$DIR/build" && ninja install) || { echo "Build failed! Aborting."; exit 1; }

# Export the plugin path so KDevelop loads our locally installed plugin
export QT_PLUGIN_PATH="$DIR/install/lib/x86_64-linux-gnu/plugins:$QT_PLUGIN_PATH"

# Launch KDevelop, defaulting to the 'craft-platforms' session.
# "$@" allows passing additional arguments from the command line.
exec kdevelop -s craft-platforms "$@"
