/*
 * Part of Very Secure FTPd
 * Licence: GPL
 * Author: Chris Evans
 * privsock.c
 *
 * This file contains code for a simple message and file descriptor passing
 * API, over a pair of UNIX sockets.
 * The messages are typically travelling across a privilege boundary, with
 * heavy distrust of messages on the side of more privilege.
 */

#include "privsock.h"

#include "utility.h"
#include "defs.h"
#include "str.h"
#include "netstr.h"
#include "sysutil.h"
#include "sysdeputil.h"
#include "session.h"

void
priv_sock_init(struct vsf_session* p_sess)
{
  const struct vsf_sysutil_socketpair_retval retval =
    vsf_sysutil_unix_stream_socketpair();
  p_sess->parent_fd = retval.socket_one;
  p_sess->child_fd = retval.socket_two;
}

void
priv_sock_send_cmd(int fd, char cmd)
{
  int retval = vsf_sysutil_write_loop(fd, &cmd, sizeof(cmd));
  if (retval != sizeof(cmd))
  {
    die("priv_sock_send_cmd");
  }
}

void
priv_sock_send_str(int fd, const struct mystr* p_str)
{
  priv_sock_send_int(fd, (int) str_getlen(p_str));
  str_netfd_write(p_str, fd);
}

char
priv_sock_get_result(int fd)
{
  char res;
  int retval = vsf_sysutil_read_loop(fd, &res, sizeof(res));
  if (retval != sizeof(res))
  {
    die("priv_sock_get_result");
  }
  return res;
}

char
priv_sock_get_cmd(int fd)
{
  char res;
  int retval = vsf_sysutil_read_loop(fd, &res, sizeof(res));
  if (retval != sizeof(res))
  {
    die("priv_sock_get_cmd");
  }
  return res;
}

void
priv_sock_get_str(int fd, struct mystr* p_dest)
{
  int retval;
  unsigned int len = (unsigned int) priv_sock_get_int(fd);
  if (len > VSFTP_PRIVSOCK_MAXSTR)
  {
    die("priv_sock_get_str: too big");
  }
  retval = str_netfd_read(p_dest, fd, len);
  if ((unsigned int) retval != len)
  {
    die("priv_sock_get_str: read error");
  }
}

void
priv_sock_send_result(int fd, char res)
{
  int retval = vsf_sysutil_write_loop(fd, &res, sizeof(res));
  if (retval != sizeof(res))
  {
    die("priv_sock_send_result");
  }
}

void
priv_sock_send_fd(int fd, int send_fd)
{
  vsf_sysutil_send_fd(fd, send_fd);
}

int
priv_sock_recv_fd(int fd)
{
  return vsf_sysutil_recv_fd(fd);
}

void
priv_sock_send_int(int fd, int the_int)
{
  int retval = vsf_sysutil_write_loop(fd, &the_int, sizeof(the_int));
  if (retval != sizeof(the_int))
  {
    die("priv_sock_send_int");
  }
}

int
priv_sock_get_int(int fd)
{
  int the_int;
  int retval = vsf_sysutil_read_loop(fd, &the_int, sizeof(the_int));
  if (retval != sizeof(the_int))
  {
    die("priv_sock_get_int");
  }
  return the_int;
}

