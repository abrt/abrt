#! /bin/sh
#echo "Running gen-version"
#./gen-version

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

