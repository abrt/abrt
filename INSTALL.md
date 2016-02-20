# How to install

### Development dependencies

Build dependencies can be listed by:

    $ ./autogen.sh sysdeps

or installed by:

    $ ./autogen.sh sysdeps --install

The dependency installer gets the data from [the rpm spec file](abrt.spec.in)

### Building from sources

When you have all dependencies installed run the following commands:

    $ ./autogen.sh --prefix=/usr \
                   --sysconfdir=/etc \
                   --localstatedir=/var \
                   --sharedstatedir=/var/lib

    $ make

or if you want to debug ABRT run:

    $ CFLAGS="-g -g3 -ggdb -ggdb3 -O0" ./autogen.sh --prefix=/usr \
                                                    --sysconfdir=/etc \
                                                    --localstatedir=/var \
                                                    --sharedstatedir=/var/lib \
                                                    --enable-debug

    $ make

### Checking

ABRT uses [Autotest](http://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.69/html_node/Using-Autotest.html)
to validate source codes. Run the test by:

    $ make check

If you want to search for memory issues, build ABRT with debug options and then
run:

    $ make maintainer-check

### Installing

If you need an rpm package, run:

    $ make rpm

otherwise run:

    $ make install
