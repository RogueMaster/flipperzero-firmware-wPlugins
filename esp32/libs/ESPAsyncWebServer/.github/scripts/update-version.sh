
#!/bin/bash
# shellcheck disable=SC2002

# fail the script if any command unexpectedly fails
set -e

if [ ! $# -eq 3 ]; then
    echo "Bad number of arguments: $#" >&2
    echo "usage: $0 <major> <minor> <patch>" >&2
    exit 1
fi

re='^[0-9]+$'
if [[ ! $1 =~ $re ]] || [[ ! $2 =~ $re ]] || [[ ! $3 =~ $re ]] ; then
    echo "error: Not a valid version: $1.$2.$3" >&2
    echo "usage: $0 <major> <minor> <patch>" >&2
    exit 1
fi

ASYNCWEBSERVER_VERSION_MAJOR="$1"
ASYNCWEBSERVER_VERSION_MINOR="$2"
ASYNCWEBSERVER_VERSION_PATCH="$3"
ASYNCWEBSERVER_VERSION="$ASYNCWEBSERVER_VERSION_MAJOR.$ASYNCWEBSERVER_VERSION_MINOR.$ASYNCWEBSERVER_VERSION_PATCH"

echo "New AsyncTCP version: $ASYNCWEBSERVER_VERSION"

echo "Updating library.properties..."
cat library.properties | sed "s/version=.*/version=$ASYNCWEBSERVER_VERSION/g" > __library.properties && mv __library.properties library.properties

echo "Updating library.json..."
cat library.json | sed "s/^  \"version\":.*/  \"version\": \"$ASYNCWEBSERVER_VERSION\",/g" > __library.json && mv __library.json library.json

echo "Updating src/AsyncWebServerVersion.h..."
cat src/AsyncWebServerVersion.h | \
sed "s/#define ASYNCWEBSERVER_VERSION_MAJOR.*/#define ASYNCWEBSERVER_VERSION_MAJOR $ASYNCWEBSERVER_VERSION_MAJOR/g" | \
sed "s/#define ASYNCWEBSERVER_VERSION_MINOR.*/#define ASYNCWEBSERVER_VERSION_MINOR $ASYNCWEBSERVER_VERSION_MINOR/g" | \
sed "s/#define ASYNCWEBSERVER_VERSION_PATCH.*/#define ASYNCWEBSERVER_VERSION_PATCH $ASYNCWEBSERVER_VERSION_PATCH/g" > src/__AsyncWebServerVersion.h && mv src/__AsyncWebServerVersion.h src/AsyncWebServerVersion.h

exit 0