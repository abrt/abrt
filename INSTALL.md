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

### Installing

If you need an rpm package, run:

    $ make rpm

otherwise run:

    $ make install

On platforms with SELinux enabled, the 'make install' command must be followed
by the 'restorecon /' command. This is not required when installing rpm
packages because rpm's selinux plugin takes care of it.
