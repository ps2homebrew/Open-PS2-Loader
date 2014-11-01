#include "include/ioman.h"
#include <kernel.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __EESIO_DEBUG
#include <sio.h>
#endif

#define MAX_IO_REQUESTS 64
#define MAX_IO_HANDLERS 64

extern void * _gp;

static int gIOTerminate = 0;

#define THREAD_STACK_SIZE   (8 * 1024)

static u8          thread_stack[THREAD_STACK_SIZE] ALIGNED(16);

struct io_request_t {
	int  type;
	void *data;
	struct io_request_t *next;
};

struct io_handler_t {
	int  type;
	io_request_handler_t handler;
};

/// Circular request queue
static struct io_request_t *gReqList;
static struct io_request_t *gReqEnd;

static struct io_handler_t gRequestHandlers[MAX_IO_HANDLERS];

static int gHandlerCount;

// id of the processing thread
static s32 gIOThreadId;

// lock for tip processing
static s32 gProcSemaId;
// lock for queue end
static s32 gEndSemaId;
// ioPrintf sema id
static s32 gIOPrintfSemaId;

static ee_thread_t gIOThread;
static ee_sema_t gQueueSema;

static int isIOBlocked = 0;
static int isIORunning = 0;

int ioRegisterHandler(int type, io_request_handler_t handler) {
	WaitSema(gProcSemaId);
	
	if (handler == NULL)
		return IO_ERR_INVALID_HANDLER;
	
	if (gHandlerCount >= MAX_IO_HANDLERS)
		return IO_ERR_TOO_MANY_HANDLERS;

	int i;
	
	for (i = 0; i < gHandlerCount; ++i) {
		if (gRequestHandlers[i].type == type)
			return IO_ERR_DUPLICIT_HANDLER;
	}
	
	gRequestHandlers[gHandlerCount].type = type;
	gRequestHandlers[gHandlerCount].handler = handler;
	gHandlerCount++;
	
	SignalSema(gProcSemaId);
	
	return IO_OK;
}

static io_request_handler_t ioGetHandler(int type) {
	int i;

	for (i = 0; i < gHandlerCount; ++i) {
		struct io_handler_t* h = &gRequestHandlers[i];
		
		if (h->type == type)
			return h->handler;
	}
	
	return NULL;
}

static void ioProcessRequest(struct io_request_t* req) {
	if (!req)
		return;
	
	io_request_handler_t hlr = ioGetHandler(req->type);
	
	// invalidate the request
	void *data = req->data;

	if (hlr)
		hlr(data);
}

static void ioWorkerThread(void *arg) {
	while (!gIOTerminate) {
		SleepThread();

		// no processing when io is blocked
		if (isIOBlocked)
			continue;
			
		// if term requested exit immediately from the loop
		if (gIOTerminate)
			break;
		
		// do we have a request in the queue?
		while (gReqList) {
			WaitSema(gProcSemaId);
			
			struct io_request_t* req = gReqList;
			ioProcessRequest(req);

			// lock the queue tip as well now
			WaitSema(gEndSemaId);
			
			// can't be sure if the request was
			gReqList = req->next;
			free(req);
						
			if (!gReqList)
				gReqEnd = NULL;

			SignalSema(gProcSemaId);
			SignalSema(gEndSemaId);
		}
	}
	
	// delete the pending requests
	while (gReqList) {
		struct io_request_t* req = gReqList;
		gReqList = gReqList->next;
		free(req); // TODO: Leak over here - we need a propper flag to free/not the user data
	}
	
	// delete the semaphores
	DeleteSema(gProcSemaId);
	DeleteSema(gEndSemaId);
	
	isIORunning = 0;

	ExitDeleteThread();
}

static void ioSimpleActionHandler(void *data) {
	io_simpleaction_t action = (io_simpleaction_t)data;
	
	if (action)
		action();
}

void ioInit(void) {
	gIOTerminate = 0;
	gHandlerCount = 0;
	gReqList = NULL;
	gReqEnd = NULL;

	gIOThreadId = 0;

	gQueueSema.init_count = 1;
	gQueueSema.max_count = 1;
	gQueueSema.option = 0;

	gProcSemaId = CreateSema(&gQueueSema);
	gEndSemaId = CreateSema(&gQueueSema);
	gIOPrintfSemaId = CreateSema(&gQueueSema);

	// default custom simple action handler
	ioRegisterHandler(IO_CUSTOM_SIMPLEACTION, &ioSimpleActionHandler);

	gIOThread.attr		= 0;
	gIOThread.stack_size	= THREAD_STACK_SIZE;
	gIOThread.gp_reg	= &_gp;
	gIOThread.func		= &ioWorkerThread;
	gIOThread.stack		= thread_stack;
	gIOThread.initial_priority = 30;

	isIORunning = 1;
	gIOThreadId = CreateThread(&gIOThread);
	StartThread(gIOThreadId, NULL);
}

int ioPutRequest(int type, void* data) {
	if (isIOBlocked)
		return IO_ERR_IO_BLOCKED;

	// check the type before queueing
	if (!ioGetHandler(type))
		return IO_ERR_INVALID_HANDLER;
	
	WaitSema(gEndSemaId);
	
	// We don't have to lock the tip of the queue...
	// If it exists, it won't be touched, if it does not exist, it is not being processed
	struct io_request_t* req = gReqEnd;
	
	if (!req) {
		gReqList = (struct io_request_t*)malloc(sizeof(struct io_request_t));
		req = gReqList;
		gReqEnd = gReqList;
	} else {
		req->next = (struct io_request_t*)malloc(sizeof(struct io_request_t));
		req = req->next;
		gReqEnd = req;
	}
	
	req->next = NULL;
	req->type = type;
	req->data = data;
	
	SignalSema(gEndSemaId);
	
	WakeupThread(gIOThreadId);
	return IO_OK;
}

int ioRemoveRequests(int type) {
	// TODO: This one needs free flag to stop leaks
	// lock the deletion sema and the queue end sema as well
	WaitSema(gEndSemaId);
	WaitSema(gProcSemaId);
	
	int count = 0;
	struct io_request_t* req = gReqList;
	struct io_request_t* last = NULL;
	
	while (req) {
		if (req->type == type) {
			struct io_request_t* next = req->next;
			
			if (last)
				last->next = next;
			
			if (req == gReqList)
				gReqList = next;
			
			if (req == gReqEnd)
				gReqEnd = last;
			
			count++;
			free(req);
			
			req = next;
		} else {
			last = req;
			req = req->next;
		}
	}
	
	SignalSema(gProcSemaId);
	SignalSema(gEndSemaId);
	
	return count;
}

void ioEnd(void) {
	// termination requested flag
	gIOTerminate = 1;
	
	// wake up and wait for end
	WakeupThread(gIOThreadId);
}

int ioGetPendingRequestCount(void) {
	int count = 0;
	
	struct io_request_t* req = gReqList;
	
	while (req) {
		count++; req = req->next; 
	}
	
	return count;
}

int ioHasPendingRequests(void) {
	return gReqList != NULL ? 1 : 0;
}

#ifdef __EESIO_DEBUG
static char tbuf[2048];
#endif

int ioPrintf(const char* format, ...) {
	WaitSema(gIOPrintfSemaId);
	
	va_list args;
	va_start(args, format);
#ifdef __EESIO_DEBUG
	int ret = vsnprintf((char *)tbuf, sizeof(tbuf), format, args);
	sio_putsn(tbuf);
#else
	int ret = vprintf(format, args);
#endif
	va_end(args);
 
	SignalSema(gIOPrintfSemaId);
	return ret;
}

int ioBlockOps(int block) {
	if (block && !isIOBlocked) {
		isIOBlocked = 1;
		
		// wait for all io to finish
		while (ioHasPendingRequests()) {
			// TODO: This is in seconds, so can be too coarse
			sleep(1);
		}
		
		// now all io should be blocked
	} else if (!block && isIOBlocked) {
		WakeupThread(gIOThreadId);
		isIOBlocked = 0;
	}

	return IO_OK;
}
