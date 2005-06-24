/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  io_unix.c: IO functions for Unix(and Unix-like systems)
 *
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 * $Id$
 *
 */





static IO **fdtable;
static unsigned long fdtable_size;

static void create_fdtable(int maxfd)
{
	fdtable = MyMalloc(sizeof(IO *)*maxfd);
	fdtable_size = maxfd;
}

static void realloc_fdtable(int maxfd)
{
	fdtable = MyRealloc(fdtable, sizeof(IO *)*maxfd);
	fdtable_size = maxfd;
}

void initIO(void)
{
	io_blockheap = BlockHeapCreate(sizeof(IO), 256);
	fde_blockheap = BlockHeapCreate(sizeof(fde), 256);

}


static int IO_nonblocking(IO *io)
{
	if(io->iotype == IO_FD)
	{
		int res;
		res = fcntl(fd, F_GETFL, 0);
		if(res != -1)
	 	{
	 		res |= O_NONBLOCK;
	 		if(fcntl(fd, F_SETFL, res) != -1)
	 		{
	 			return 1;
	 		} 
	 	
	 	}
	} else {
		return 1;
	} 
	return -1;	 

}

IO *FD_to_IO(int fd)
{
	return(fdtable[fd]);		
}

int IO_select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{ 
	return(select(n, readfds, writefds, expectfds, timeout));
}


IO *IO_accept(IO *io, struct sockaddr *sock, int len)
{
 	if(io->iotype == IO_FD)
 	{
 	 	int fd;
 	 	IO *newio;
 	 	fd = accept(io->ioh->F->read_fd, sock, &len);
 	 	if(fd < 0)
	 	 	return NULL;
 		newio = IO_newfd(fd, FD_SOCKET);
		if(IO_nonblocking(io) == -1)
		{
			ilog(L_CRIT, "IO_socket: Couldn't set FD %d non blocking: %s", fd, strerror(errno));
			IO_close(io);
			return NULL; 
		}
		return(newio):
 	}
	errno = EINVAL;
	return NULL;   
}	

/*
 * IO *IO_newfd(int fd, int fdtype)
 *
 * Input: File descriptor and FD type to create an IO handle for
 * Ouput: The newly allocated IO handle
 */
IO *IO_newfd(int fd_read, int fd_write, int fdtype)
{
	io = BlockHeapAlloc(io_blockheap);
	memset(io, 0, sizeof(IO));
	io->iotype = IO_FD;
	io->ioh->F = BlockHeapAlloc(fde_blockheap);
	memset(io->ioh->F, 0, sizeof(fde));
	io->ioh->F->fd_read = fd_read;
	io->ioh->F->fd_write = fd_write;
	io->ioh->F->type = fdtype;
	if(fd_read == fd_write)
	{
		fdtable[fd_read] = io;
	} else {
		fdtable[fd_read] = io;
		fdtable[fd_write] = io;
	}
	return(io);
}

/*
 * IO *IO_open(const char *file, int flags, int mode)
 *
 * Input: Filename to open and any flags to go with it
 * Ouput: IO handle pointing to the file
 */
 
IO *IO_open(const char *file, int flags, ...)
{
	int fd, mode;
	
	va_list ap;
	va_start(ap, flags);
	mode = va_arg(ap, int);
	fd = open(file, flags, mode);
	if(fd < 0)
	{
		return NULL;
	}
	return(IO_newfd(fd, fd, FD_FILE));
}

/*
 * IO_pipepair - Creates an IO handle with pipes on the backend 
 *
 * Input: An array of fds that will be passed to new process
 * Ouput: IO handle to be used by the process
 */
 

IO *IO_pipepair(int pass_fd[2])
{
	IO *io;
	int fdr[2], fdw[2];
	
	if(pipe(fdr) == -1)
		return NULL;
	if(pipe(fdw) == -1)
	{
		close(fdr[0]);
		close(fdr[1]);
		return NULL;
	}
	passfd[0] = fdw[0];
	passfd[1] = fdr[1];
	return(IO_newfd(fdr[0], fdw[1], FD_PIPE));
}

IO *IO_socketpair(int family, int sock_type, int protocol, int *remotefd)
{
	IO *io;
	int fd[2];
	
	if(socketpair(family, sock_type, protocol, fd) == -1)
		return NULL;
	
	*remotefd = fd[1];
	return(IO_newfd(fd[0], fd[0], FD_SOCKET));		
}



IO *IO_socket(int family, int sock_type, int proto)
{
	int fd;
	IO *io;
	fd = socket(family, sock_type, int proto) 
	if(fd < 0)
	{
		return NULL;
	}
	io = IO_newfd(fd, fd, FD_SOCKET);
	if(IO_nonblocking(io) == -1)
	{
		ilog(L_CRIT, "IO_socket: Couldn't set FD %d non blocking: %s", fd, strerror(errno));
		IO_close(io);
		return NULL;
	}
	return IO;
}

int IO_write(IO *io, void *data, size_t len)
{
	switch(io->iotype)
	{
		case IO_FD:
		{
			return(write(io->ioh->F->fd_write, data, len));
		}
	}
	errno = EINVAL;
	return -1;
}

int IO_read(IO *io, void *data, size_t len)
{
	switch(io->iotype)
	{
		case IO_FD:
		{
			return(read(io->ioh->F->fd_read, data, len));
		}
	}
	errno = EINVAL;
	return -1;
}

void IO_close(IO *io, void *data, size_t len)
{
	switch(io->iotype)
	{
		case IO_FD:
		{
			if(io->ioh->F->fd_read != io->ioh->F->fd_write)
			{
				close(io->ioh->F->fd_read);
				close(io->ioh->F->fd_write);
				fdtable[io->ioh->F->fd_read] = NULL;
				fdtable[io->ioh->F->fd_write] = NULL;
			} else {
				close(io->ioh->F->fd_read);
				fdtable[io->ioh->F->fd_read] = NULL;
			}
			BlockHeapFree(fde_heap, F);
		}
	}
	BlockHeapFree(io_heap, io);
}


