#!/bin/sh

print_help()
{
cat << EOH
Prepares the source tree for configuration

Usage:
  autogen.sh [sysdeps [--install]]

Options:

  sysdeps          prints out all dependencies
    --install      install all dependencies ('sudo yum install \$DEPS')

EOH
}

parse_build_requires_from_spec_file()
{
    PACKAGE=$1
    rpmspec -P $PACKAGE.spec | grep "^\(Build\)\?Requires:" | \
        tr -s " " | tr "," "\n" | cut -f2- -d " " | \
        grep -v "\(^\|python[23]-\)"$PACKAGE | sort -u | sed -E 's/^(.*) (.*)$/"\1 \2"/' | tr \" \'
}

list_build_dependencies()
{
    local BUILD_SYSTEM_DEPS_LIST="gettext-devel make tito"
    echo $BUILD_SYSTEM_DEPS_LIST $(parse_build_requires_from_spec_file abrt)
}

case "$1" in
    "--help"|"-h")
            print_help
            exit 0
        ;;
    "sysdeps")
            DEPS_LIST=$(list_build_dependencies)
            if [ "$2" == "--install" ]; then
                set -x verbose
                eval sudo dnf --assumeyes install --setopt=strict=0 $DEPS_LIST
                set +x verbose
            else
                echo $DEPS_LIST
            fi
            exit 0
        ;;
    *)
            echo "Running gen-version"
            ./gen-version

            echo "Running autoreconf"
            autoreconf --install --force --verbose || exit 1

            echo "Running intltoolize..."
            intltoolize --force --copy --automake || exit 1

            if [ -z "$NOCONFIGURE" ]; then
                echo "Running configure ..."
                if [ 0 -eq $# ]; then
                    ./configure \
                        --prefix=/usr \
                        --mandir=/usr/share/man \
                        --infodir=/usr/share/info \
                        --sysconfdir=/etc \
                        --localstatedir=/var \
                        --sharedstatedir=/var/lib \
                        --enable-native-unwinder \
                        --enable-dump-time-unwind \
                        --enable-debug
                    echo "Configured for local debugging ..."
                else
                    ./configure "$@"
                fi
            fi
        ;;
esac
