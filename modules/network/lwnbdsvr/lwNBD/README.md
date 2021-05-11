#lwNBD

* Description : A Lightweight NBD server based on lwIP stack
* Official repository : https://github.com/bignaux/lwNBD
* Author : Ronan Bignaux
* Licence : BSD
* lwIP 2.0.0 Socket API

#History

On Playstation 2, there is no standardised central partition table like GPT for hard disk partitioning, nor is there a standard file system but PFS and HDLoader. In fact, there are few tools capable of handling hard disks, especially under Linux, and the servers developed in the past to handle these disks via the network did not use a standard protocol, which required each software wishing to handle the disks to include a specific client part, which were broken when the toolchain was updated. The same goes for the memory cards and other block devices on this console, which is why I decided to implement NBD on this target first.

lwNBD is developed on MIPS R3000 IOP in [my OPL nbd branch](https://github.com/bignaux/Open-PS2-Loader/tree/nbd/modules/network/lwnbdsvr), that provides a good uptodate use case.

#Status

Although this server is not yet complete in respect of the minimal requirements defined by the [NBD protocol](https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md#baseline), it is nevertheless usable with certain clients, in particular nbdfuse (provided by libnbd), the client I am using for this development. In a [RERO spirit](https://en.wikipedia.org/wiki/Release_early,_release_often) i publish this "AS-IS".

#TODO:
* fix NBD_FLAG_FIXED_NEWSTYLE negotiation
* NBD_OPT_INFO/NBD_OPT_GO, the server is not yet able to serve many export. Currently, the server takes the first on the list.