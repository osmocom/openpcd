#ifndef _VTY_H
#define _VTY_H

#include <stdio.h>
#include <stdarg.h>

/* GCC have printf type attribute check.  */
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif				/* __GNUC__ */

/* Does the I/O error indicate that the operation should be retried later? */
#define ERRNO_IO_RETRY(EN) \
	(((EN) == EAGAIN) || ((EN) == EWOULDBLOCK) || ((EN) == EINTR))

/* Vty read buffer size. */
#define VTY_READ_BUFSIZ 512

#define VTY_BUFSIZ 512
#define VTY_MAXHIST 20

struct vty {
	FILE *file;

	/* File descripter of this vty. */
	int fd;

	/* Is this vty connect to file or not */
	enum { VTY_TERM, VTY_FILE, VTY_SHELL, VTY_SHELL_SERV } type;

	/* Node status of this vty */
	int node;

	/* Failure count */
	int fail;

	/* Output buffer. */
	struct buffer *obuf;

	/* Command input buffer */
	char *buf;

	/* Command cursor point */
	int cp;

	/* Command length */
	int length;

	/* Command max length. */
	int max;

	/* Histry of command */
	char *hist[VTY_MAXHIST];

	/* History lookup current point */
	int hp;

	/* History insert end point */
	int hindex;

	/* For current referencing point of interface, route-map,
	   access-list etc... */
	void *index;

	/* For multiple level index treatment such as key chain and key. */
	void *index_sub;

	/* For escape character. */
	unsigned char escape;

	/* Current vty status. */
	enum { VTY_NORMAL, VTY_CLOSE, VTY_MORE, VTY_MORELINE } status;

	/* Window width/height. */
	int width;
	int height;

	/* Configure lines. */
	int lines;

	int monitor;

	/* In configure mode. */
	int config;
};

/* Small macro to determine newline is newline only or linefeed needed. */
#define VTY_NEWLINE  ((vty->type == VTY_TERM) ? "\r\n" : "\n")

static inline char *vty_newline(struct vty *vty)
{
	return VTY_NEWLINE;	
}

/* Prototypes. */
void vty_init (void);
void vty_init_vtysh (void);
void vty_reset (void);
struct vty *vty_new (void);
struct vty *vty_create (int vty_sock);
int vty_out (struct vty *, const char *, ...) PRINTF_ATTRIBUTE(2, 3);
int vty_out_newline(struct vty *);
int vty_read(struct vty *vty);
void vty_read_config (char *, char *);
void vty_time_print (struct vty *, int);
void vty_close (struct vty *);
char *vty_get_cwd (void);
void vty_log (const char *level, const char *proto, const char *fmt, va_list);
int vty_config_lock (struct vty *);
int vty_config_unlock (struct vty *);
int vty_shell (struct vty *);
int vty_shell_serv (struct vty *);
void vty_hello (struct vty *);


#endif
