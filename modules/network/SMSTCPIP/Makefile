IOP_BIN = SMSTCPIP.irx
IOP_OBJS = ps2ip.o inet.o ip.o ip_addr.o ip_frag.o etharp.o tcp_in.o tcp_out.o \
	tcp.o tcpip.o mem.o api_lib.o api_msg.o sockets.o netif.o udp.o memp.o \
	icmp.o pbuf.o timers.o exports.o imports.o
IOP_LIBS =

IOP_INCS += -Iinclude -I../../iopcore/common
IOP_CFLAGS += -DLWIP_NOASSERT -Wall -fno-builtin-printf
IOP_ASFLAGS +=
IOP_LDFLAGS += -s

ifeq ($(INGAME_DRIVER),1)
IOP_CFLAGS += -DINGAME_DRIVER
endif

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make
