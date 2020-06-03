# Making changes in ABRT

Before you start hacking on ABRT, please, make sure you can build ABRT from
source codes as described in [the installation guide](INSTALL.md).

## Where to make your changes

    * augeas - ABRT configuration parsers
    * init-scripts - SysV init scripts and systemd services
    * src/applet - Desktop notifications daemon
    * src/cli - Command-line interface (abrt)
    * src/configuration-gui - Source codes of system-config-abrt
    * src/daemon - Source codes of abrtd
    * src/dbus - Source codes of abrt-dbus and Problems2 implementation
    * src/hooks - Core dump handler, Python sys.excepthook, ...
    * src/plugins - Tools analyzing problem data collected by the hooks
    * src/lib - Functionality shared among abrtd, the hooks and the plugins
    * src/python-problem - Modern Python interface to ABRT problem database
    * tests - Unit tests
    * tests/runtest - Integration tests

## How to check your changes

ABRT uses [Autotest](http://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.69/html_node/Using-Autotest.html)
to validate source codes. When you done with code changes and 'make' succeeded
you should run the Autotest tests to verify you didn't break anything
unexpected:

    $ make check

If you want to search for memory issues, build ABRT with debug options and then
run:

    $ make maintainer-check

You can also run the testsuite manually from the tests directory:

    $ cd tests
    $ make testsuite
    $ ./testsuite

If there is a crashing test and you want to run the test program under GDB, you
can go to the directory with failing test (./testsuite.dir/[TEST NAME]) and use
libtool to execute the test with gdb:

    $ libtool --mode=execute gdb ./[TESTNAME]

When creating a new test please use macros defined in
tests/helpers/testsuite.h. The C compiler is configured to include files form
the tests/helpers directory.

## Verify ABRT plays nice with the rest of OS

ABRT integration tests live in the test/runtest directory. The test are based
on [BeakerLib](https://github.com/beakerlib/beakerlib).

Caution! It is not recommended to run the tests unless you really know what
you are doing. Lot of tests changes configuration of the system they are
running on and can be potentially harmful to the system upon failures.

The tests are supposed to be run automatically on a solo virtual machine but it
is possible to run them on development machines. If you decide to update an
existing test or add a new test you will definitely need to check if the test is
OK.

Simple test should have the following directory structure:
    simple_test/PURPOSE
    simple_test/runtest.sh

and a minimal `runtest.sh` would look like the following:

```bash
. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

# Do not forget to update the TEST variable.
TEST="my-test-name"

PACKAGE="abrt"

# Add as many helper variables as you need.

rlJournalStart
    rlPhaseStartSetup
        # Avoid interference with previous tests.
        check_prior_crashes

        # Run the test in a temporary directory.
        TmpDir=$(mktemp -d)
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "Simple test"
        # Make ABRT ready for detecting new crashes. Basically, this function
        # just removes the /tmp/abrt-done file.
        prepare

        # Produce an artificial crash. If everything is OK, abrtd will execute
        # events from the /etc/libreport/events.d/test_event.conf file which is
        # created by the tests/runtests/aux/pre.sh file.
        # The pre.sh file is executed automatically on headless test machines
        # and if you want to run a test on a development machine you must
        # create the event configuration file manually.
        generate_crash

        # Waint until the /tmp/abrt-done file is created or die on timeout.
        wait_for_hooks

        # Get the file system path of the last crash.
        get_crash_path

        # Use any bash construction, BeakerLib function or function form
        # the test/runtests/aux/lib.sh file to verify the contents of
        # the crash directory.
    rlPhaseEnd

    rlPhaseStartCleanup
        # Bundle logs. All files with the .log suffix will be bundled.
        rlBundleLogs abrt $(echo *.log)

        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd

    rlJournalPrintText
rlJournalEnd
```
