# How to install

### Development dependencies

Build dependencies can be listed by:

    $ ./autogen.sh sysdeps

or installed by:

    $ ./autogen.sh sysdeps --install

The dependency installer gets the data from [the rpm spec file](abrt.spec.in)

### Building from sources

When you have all dependencies installed run the following commands:

    $ tito build --rpm --test

Note: You have to have your changes committed. Tito doesn't deal in dirty trees.

or if you want to debug ABRT run:

    $ CFLAGS="-g -g3 -ggdb -ggdb3 -O0" ./autogen.sh --prefix=/usr \
                                                    --sysconfdir=/etc \
                                                    --localstatedir=/var \
                                                    --sharedstatedir=/var/lib \
                                                    --enable-debug

    $ make

### Installing

    $ tito build --rpm --test -i

