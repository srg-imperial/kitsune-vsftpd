/*
 * Part of Very Secure FTPd
 * Licence: GPL
 * Author: Chris Evans
 * tcpwrap.c
 *
 * Routines to encapsulate the usage of tcp_wrappers.
 */

#include "tcpwrap.h"
#include "builddefs.h"

#ifndef VSF_BUILD_TCPWRAPPERS

int
vsf_tcp_wrapper_ok(const struct vsf_sysutil_sockaddr* p_addr)
{
  (void) p_addr;
  return 1;
}

#else /* VSF_BUILD_TCPWRAPPERS = yes */

#include <tcpd.h>
#include <sys/syslog.h>

int deny_severity = LOG_WARNING;
int allow_severity = LOG_INFO;

int
vsf_tcp_wrapper_ok(const struct vsf_sysutil_sockaddr* p_addr)
{
  struct request_info req;
  request_init(&req, RQ_DAEMON, "vsftpd", RQ_CLIENT_SIN, (void*)p_addr, 0);
  fromhost(&req);
  if (!hosts_access(&req))
  {
    return 0;
  }
  return 1;
}

#endif /* VSF_BUILD_TCPWRAPPERS */

