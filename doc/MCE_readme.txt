	Background

MCEs can be fatal (they panic kernel) or not.
Fatal MCE are delivered as exception#18.
Non-fatal ones sometimes are delivered as exception#18; other times
they are silently recorded in magic MSRs, CPU is not alerted.
Linux kernel periodically (up to 5 mins interval) reads those MSRs
and if MCE is seen there, it is piped in binary form through
/dev/mcelog to whoever listens on it. (Such as mcelog tool in
--daemon mode; but cat </dev/mcelog would do too).

"Machine Check Exception:" message is printed *only* by fatal MCEs.
It will be caught as vmcore if kdump is configured.

Non-fatal MCEs have "[Hardware Error]: Machine check events logged"
message in kernel log.
When /dev/mcelog is read, *no additional kernel log messages appear*.

> Are those magic MSR registers cleared when read via /dev/mcelog?

Yes.

> Without mcelog utility, we can directly read only binary form, right?
> Not nice, but still useful, right?
> (could be transferred to nice text form on other machine).

No, raw /dev/mcelog data is not easy to interpret on other machine.
In fact, it can't be used by mcelog tool even on the same machine.
Technical reason is that mcelog uses an obscure ioctl on /dev/mcelog
in order to know the size of binary blob with MCE information.
When run on a file, ioctl fails, and mcelog bombs out.

Looks like without mcelog running and processing /dev/mcelog data,
non-fatal MCE's can't be easily decoded with currently existing tools.

mcelog tool can be configured to write log to /var/log/mcelog
(RHEL6 does that) or to syslog (RHEL7 does that).


	How ABRT catches MCEs

Fatal MCEs are caught as any fatal kernel panic is caught - as a vmcore.
The oops text, which goes to "backtrace" element, will be the decoded
MCE message from kernel log buffer.

Non-fatal MCEs are caught as kernel oopses.
If "Machine check events logged" message is seen in "dmesg" element,
we assume it's a MCE, and create "not-reportable" element with suitable
explanation.
Then we check whether /var/log/mcelog exists,
or whether system log contains "mcelog: Hardware event",
and create a "comment" element with explanatory text, followed by
last 20 lines from either of those files.


	How to test MCEs

There is an MCE injection tool and a kernel module, both named mce-inject.
(The tool comes from mce-test project, may be found in ras-utils RHEL7 package).
The script I used is:

modprobe mce-inject
sync &
sleep 1
sync
# This can crash the machine:
echo "Injecting MCE from file $1"
mce-inject "$1"
echo "Exitcode:$?"

It requires files which describe MCE to simulate. I grabbed a few examples
from mce-test.tar.gz (source tarball of mce-test project).
I used this file to cause a non-fatal MCE:

CPU 0 BANK 2
STATUS VAL OVER EN

And this one to cause a fatal one:

CPU 0 BANK 4
MCGSTATUS MCIP
STATUS FATAL S
RIP 12343434
MISC 11

(Not sure what failures exactly they imitate, maybe there are better examples).


For testing fatal MCEs you need to set up kdump. Mini-recipe:
(1) yum install --enablerepo='*debuginfo*' kexec-tools crash kernel-debuginfo
(2) add "crashkernel=128M" to the kernel's command line, reboot
(3) before injecting fatal MCE, start kdump service:
    systemctl start kdump.service
