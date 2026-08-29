
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

ASYNCTCP_VERSION_MAJOR="$1"
ASYNCTCP_VERSION_MINOR="$2"
ASYNCTCP_VERSION_PATCH="$3"
ASYNCTCP_VERSION="$ASYNCTCP_VERSION_MAJOR.$ASYNCTCP_VERSION_MINOR.$ASYNCTCP_VERSION_PATCH"

echo "New AsyncTCP version: $ASYNCTCP_VERSION"

echo "Updating library.properties..."
cat library.properties | sed "s/version=.*/version=$ASYNCTCP_VERSION/g" > __library.properties && mv __library.properties library.properties

echo "Updating library.json..."
cat library.json | sed "s/^  \"version\":.*/  \"version\": \"$ASYNCTCP_VERSION\",/g" > __library.json && mv __library.json library.json

echo "Updating src/AsyncTCPVersion.h..."
cat src/AsyncTCPVersion.h | \
sed "s/#define ASYNCTCP_VERSION_MAJOR.*/#define ASYNCTCP_VERSION_MAJOR $ASYNCTCP_VERSION_MAJOR/g" | \
sed "s/#define ASYNCTCP_VERSION_MINOR.*/#define ASYNCTCP_VERSION_MINOR $ASYNCTCP_VERSION_MINOR/g" | \
sed "s/#define ASYNCTCP_VERSION_PATCH.*/#define ASYNCTCP_VERSION_PATCH $ASYNCTCP_VERSION_PATCH/g" > src/__AsyncTCPVersion.h && mv src/__AsyncTCPVersion.h src/AsyncTCPVersion.h

exit 0