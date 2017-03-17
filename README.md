OpenPCD, OpenPICC and SIMtrace device firmware
==============================================

This repository contains the C-language firmware of a couple of
different USB devices that share nothing in common but the fact that
they contain and Atmel AT91SAM7S microcontroller and that Harald Welte
was involved in their development.

The OpenPCD 1.x and OpenPICC 1.x devices are pretty much obsolete these
days, so SAM7S based SIMtrace 1.x is the only relevant platform these
days.

[Osmocom](https://osmocom.org/)
[SIMtrace](https://osmocom.org/projects/simtrace) is a USB-attached
peripheral device that is primarily used to sniff the traffic between a
SIM/USIM card and a Phone or cellular modem.

Homepage
--------

The official homepage of the project is
<https://osmocom.org/projects/simtrace/wiki/>

GIT Repository
--------------

You can clone from the official openpcd.git repository using

	git clone git://git.osmocom.org/openpcd.git

There is a cgit interface at <http://git.osmocom.org/openpcd/>

Documentation
-------------

Ther homepage (see above) contains a wiki with information as well as
the SIMtrace user manual.

Mailing List
------------

Discussions related to SIMtrace are happening on the
simtrace@lists.osmocom.org mailing list, please see
<https://lists.osmocom.org/mailman/listinfo/simtrace> for subscription
options and the list archive.

Please observe the [Osmocom Mailing List
Rules](https://osmocom.org/projects/cellular-infrastructure/wiki/Mailing_List_Rules)
when posting.

Contributing
------------

Our coding standards are described at
<https://osmocom.org/projects/cellular-infrastructure/wiki/Coding_standards>

We use accept code/patch submissions via e-mail to the above-mentioned
mailing list.
