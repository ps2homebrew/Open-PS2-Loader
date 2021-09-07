# lwNBD

*   Description : A Lightweight NBD server
*   Official repository : <https://github.com/bignaux/lwNBD>
*   Author : Ronan Bignaux
*   Licence : BSD
*   Socket API: lwIP 2.0.0 and Linux supported.

Targeting first the use on Playstation 2 IOP, a 37.5 MHz MIPS processor
and 2 MB of RAM, lwNBD is designed to run on bare metal or OS embedded system.
With modulararity and portability in mind, it is developed according to several
code standards, including :

*   [Object-Oriented Programming in C](https://github.com/QuantumLeaps/OOP-in-C/)
*   nbdkit implementation naming, using a verbatim copy of their nbd-protocol.h

There are 2 examples provided :

*   lwnbd_linux.c is a simple nbd server, that use stdio_d.c driver to
    serve a list of files as command line parameters.
*   lwnbdsvr.c an IRX module for [Open-PS2-Loader](https://github.com/ps2homebrew/Open-PS2-Loader).
    It can export hdd drive.

## History

On Playstation 2, there is no standardized central partition table like GPT for
hard disk partitioning, nor is there a standard file system but PFS and
HDLoader. In fact, there are few tools capable of handling hard disks,
especially under Linux, and the servers developed in the past to handle these
disks via the network did not use a standard protocol, which required each
software wishing to handle the disks to include a specific client part,
which were broken when the toolchain was updated. The same goes for the memory
cards and other block devices on this console, which is why I decided to
implement NBD on this target first.

## Status

Although this server is not yet complete in respect of the minimal requirements
defined by the [NBD protocol](https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md#baseline),
it is nevertheless usable with certain clients. In a [RERO spirit](https://en.wikipedia.org/wiki/Release_early,_release_often)
i publish this "AS-IS".

Known supported clients :

*   nbdfuse (provided by libnbd), works on windows with WSL2.
*   nbd-client
*   Ceph for Windows (wnbd-client.exe)

## TODO

*   event loop based on select()
*   provide a clean API
*   fix NBD_FLAG_FIXED_NEWSTYLE negotiation
*   NBD_OPT_INFO/NBD_OPT_GO, the server is not yet able to serve many export or change blocksize.
