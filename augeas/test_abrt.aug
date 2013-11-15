module Test_abrt =

    let conf ="# Enable this if you want abrtd to auto-unpack crashdump tarballs which appear
# in this directory (for example, uploaded via ftp, scp etc).
# Note: you must ensure that whatever directory you specify here exists
# and is writable for abrtd. abrtd will not create it automatically.
#
#WatchCrashdumpArchiveDir = /var/spool/abrt-upload

# Max size for crash storage [MiB] or 0 for unlimited
#
MaxCrashReportsSize = 1000

# Specify where you want to store coredumps and all files which are needed for
# reporting. (default:/var/tmp/abrt)
#
# Changing dump location could cause problems with SELinux. See man abrt_selinux(8).
#
#DumpLocation = /var/tmp/abrt

# If you want to automatically clean the upload directory you have to tweak the
# selinux policy.
#
DeleteUploaded = no

# A name of event which is run automatically after problem's detection. The
# event should perform some fast analysis and exit with 70 if the
# problem is known.
#
# In order to run this event automatically after detection, the
# AutoreportingEnabled option must be configured to 'yes'
#
# Default value: report_uReport
#
AutoreportingEvent = report_uReport

# Enables automatic running of the event configured in AutoreportingEvent option.
#
AutoreportingEnabled = no

# Enables shortened GUI reporting where the reporting is interrupted after
# AutoreportingEvent is done.
#
# Default value: Yes but only if application is running in GNOME desktop
#                session; otherwise No.
#
# ShortenedReporting = yes
"

    test Abrt.lns get conf =
        { "#comment" = "Enable this if you want abrtd to auto-unpack crashdump tarballs which appear" }
        { "#comment" = "in this directory (for example, uploaded via ftp, scp etc)." }
        { "#comment" = "Note: you must ensure that whatever directory you specify here exists" }
        { "#comment" = "and is writable for abrtd. abrtd will not create it automatically." }
        { "#comment" = "" }
        { "#comment" = "WatchCrashdumpArchiveDir = /var/spool/abrt-upload" }
        {}
        { "#comment" = "Max size for crash storage [MiB] or 0 for unlimited" }
        { "#comment" = "" }
        { "MaxCrashReportsSize" = "1000" }
        {}
        { "#comment" = "Specify where you want to store coredumps and all files which are needed for" }
        { "#comment" = "reporting. (default:/var/tmp/abrt)" }
        { "#comment" = "" }
        { "#comment" = "Changing dump location could cause problems with SELinux. See man abrt_selinux(8)." }
        { "#comment" = "" }
        { "#comment" = "DumpLocation = /var/tmp/abrt" }
        {}
        { "#comment" = "If you want to automatically clean the upload directory you have to tweak the" }
        { "#comment" = "selinux policy." }
        { "#comment" = "" }
        { "DeleteUploaded" = "no" }
        {}
        { "#comment" = "A name of event which is run automatically after problem's detection. The" }
        { "#comment" = "event should perform some fast analysis and exit with 70 if the" }
        { "#comment" = "problem is known." }
        { "#comment" = "" }
        { "#comment" = "In order to run this event automatically after detection, the" }
        { "#comment" = "AutoreportingEnabled option must be configured to 'yes'" }
        { "#comment" = "" }
        { "#comment" = "Default value: report_uReport" }
        { "#comment" = "" }
        { "AutoreportingEvent" = "report_uReport" }
        {}
        { "#comment" = "Enables automatic running of the event configured in AutoreportingEvent option." }
        { "#comment" = "" }
        { "AutoreportingEnabled" = "no" }
        {}
        { "#comment" = "Enables shortened GUI reporting where the reporting is interrupted after" }
        { "#comment" = "AutoreportingEvent is done." }
        { "#comment" = "" }
        { "#comment" = "Default value: Yes but only if application is running in GNOME desktop" }
        { "#comment" = "session; otherwise No." }
        { "#comment" = "" }
        { "#comment" = "ShortenedReporting = yes" }
