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
    grep "^\(Build\)\?Requires:" $PACKAGE.spec.in | grep -v "%{name}" | tr -s " " | tr "," "\n" | cut -f2 -d " " | grep -v "^"$PACKAGE | sort -u | while read br;
    do
        if [ "%" = ${br:0:1} ]; then
            grep "%define $(echo $br | sed -e 's/%{\(.*\)}/\1/')" $PACKAGE.spec.in | sed -e 's/^[ \t]*//' | tr -s " " | cut -f3 -d" "
        else
            echo $br
        fi
    done | tr "\n" " "
}

list_build_dependencies()
{
    local BUILD_SYSTEM_DEPS_LIST="gettext-devel"
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
                sudo dnf install --setopt=strict=0 $DEPS_LIST
                set +x verbose
            else
                echo $DEPS_LIST
            fi
            exit 0
        ;;
    *)
            echo "Running gen-version"
            ./gen-version

            mkdir -p m4
            echo "Creating m4/aclocal.m4 ..."
            test -r m4/aclocal.m4 || touch m4/aclocal.m4

            echo "Running autopoint"
            autopoint --force || exit 1

            echo "Running intltoolize..."
            intltoolize --force --copy --automake || exit 1

            echo "Running aclocal..."
            aclocal || exit 1

            echo "Running libtoolize..."
            libtoolize || exit 1

            echo "Running autoheader..."
            autoheader || return 1

            echo "Running autoconf..."
            autoconf --force || exit 1

            echo "Running automake..."
            automake --add-missing --force --copy || exit 1

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
        ;;
esac
