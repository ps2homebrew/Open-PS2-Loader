
#include <stdio.h>
#include <sysclib.h>
#include <ps2ip.h>

#include "poll.h"

#define POLL_CAN_READ  (POLLIN | POLLRDNORM)
#define POLL_CAN_WRITE (POLLOUT | POLLWRNORM | POLLWRBAND)
#define POLL_HAS_EXCP  (POLLRDBAND | POLLPRI)

#define POLL_EVENTS_MASK (POLL_CAN_READ | POLL_CAN_WRITE | POLL_HAS_EXCP)

int poll(struct pollfd *fds, unsigned long nfds, int timeout)
{
    int i,err;
    fd_set rfd, wfd, efd, ifd;
    struct timeval timebuf;
    struct timeval *tbuf = (struct timeval *)0;
    int n = 0;
    int count;
    
    FD_ZERO(&ifd);
    FD_ZERO(&rfd);
    FD_ZERO(&wfd);
    FD_ZERO(&efd);

    for (i = 0; i < nfds; i++) {
		int events = fds[i].events;
		int fd = fds[i].fd;

		fds[i].revents = 0;

		if ((fd < 0) || (FD_ISSET(fd, &ifd)))
	   		continue;

		if (fd > n)
	    	n = fd;

		if (events & POLL_CAN_READ)
	    	FD_SET(fd, &rfd);

		if (events & POLL_CAN_WRITE)
	    	FD_SET(fd, &wfd);

		if (events & POLL_HAS_EXCP)
	    	FD_SET(fd, &efd);
    }

    if (timeout >= 0) {
		timebuf.tv_sec = timeout / 1000;
		timebuf.tv_usec = (timeout % 1000) * 1000;
		tbuf = &timebuf;
    }

    err = lwip_select(n+1, &rfd, &wfd, &efd, tbuf);

    if (err < 0)
		return err;
  
   	count = 0;
   	
   	for (i = 0; i < nfds; i++) {
		int revents = (fds[i].events & POLL_EVENTS_MASK);
		int fd = fds[i].fd;

		if (fd < 0)
	   		continue;

		if (FD_ISSET(fd, &ifd))
	   		revents = POLLNVAL;
		else {
	   		if (!FD_ISSET(fd, &rfd))
	       		revents &= ~POLL_CAN_READ;

	   		if (!FD_ISSET(fd, &wfd))
	       		revents &= ~POLL_CAN_WRITE;

	   		if (!FD_ISSET(fd, &efd))
	       		revents &= ~POLL_HAS_EXCP;
		}

		if ((fds[i].revents = revents) != 0)
	   		count++;
    }
    
    return count; 
}

