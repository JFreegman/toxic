#
# Regular cron jobs for the toxic package
#
0 4	* * *	root	[ -x /usr/bin/toxic_maintenance ] && /usr/bin/toxic_maintenance
