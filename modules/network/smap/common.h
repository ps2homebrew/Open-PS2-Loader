//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTF(args...) printf(args)
#else
#define DEBUG_PRINTF(args...)
#endif

#define MAX_FRAME_SIZE	1600
#define SMAP_RPC_BLOCK_SIZE	32

struct PacketTag{
	unsigned int length;
	unsigned int offset;
};

struct IncomingPacketReqs{
	unsigned int NumPackets;
	unsigned int TotalLength;
	struct PacketTag tags[SMAP_RPC_BLOCK_SIZE];
};

