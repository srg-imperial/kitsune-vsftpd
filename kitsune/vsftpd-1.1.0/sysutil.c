/*
 * Part of Very Secure FTPd
 * Licence: GPL
 * Author: Chris Evans
 * 
 * sysutil.c
 *
 * Routines to make the libc/syscall API more pleasant to use. Long term,
 * more libc/syscalls will go in here to reduce the number of .c files with
 * dependencies on libc or syscalls.
 */

#define PRIVATE_HANDS_OFF_syscall_retval syscall_retval
#define PRIVATE_HANDS_OFF_exit_status exit_status
#include "sysutil.h"
#include "utility.h"

/* Activate 64-bit file support on Linux/32bit */
#define _FILE_OFFSET_BITS 64

/* For Linux, this adds nothing :-) */
#include "port/porting_junk.h"

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/time.h>
/* Must be before netinet/ip.h. Found on FreeBSD, Solaris */
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <limits.h>

/* Private variables to this file */
/* Current umask() */
static unsigned int s_current_umask;
/* Cached time */
static struct timeval s_current_time;
/* Current pid */
static int s_current_pid = -1;

/* Our internal signal handling implementation details */
static struct vsf_sysutil_sig_details
{
  vsf_sighandle_t sync_sig_handler;
  void* p_private;
  int pending;
  int running;
} s_sig_details[NSIG];

static vsf_context_io_t s_io_handler;
static void* s_p_io_handler_private;
static int s_io_handler_running;

/* File locals */
static void vsf_sysutil_common_sighandler(int signum);
static int vsf_sysutil_translate_sig(const enum EVSFSysUtilSignal sig);
static void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int));
static int vsf_sysutil_translate_memprot(
  const enum EVSFSysUtilMapPermission perm);
static int vsf_sysutil_translate_openmode(
  const enum EVSFSysUtilOpenMode mode);
static void vsf_sysutil_alloc_statbuf(struct vsf_sysutil_statbuf** p_ptr);

static void
vsf_sysutil_common_sighandler(int signum)
{
  if (signum < 0 || signum >= NSIG)
  {
    bug("signal out of range in vsf_sysutil_common_sighandler");
  }
  if (s_sig_details[signum].sync_sig_handler)
  {
    s_sig_details[signum].pending = 1;
  }
}

/* Kitsune hack to deal with looped blocking */
static char *update_point;
void vsf_sysutil_kitsune_set_update_point(char *upd_point) {
  update_point = upd_point;
}

/* Notes. This signal check is evaluated after potentially blocking system
 * calls, i.e. at highly defined points in the code. Since we only interrupt
 * at these definite locations, the signal handlers can be non-trivial
 * without us having to worry about re-entrancy.
 *
 * We guarantee that a handler for a given signal is not re-entrant. This
 * is taken care of by the "running" flag.
 *
 * This call itself can only be re-entered once we dereference the actual
 * hander function pointer, so we are safe with respect to races modifying
 * the "running" flag.
 */
void
vsf_sysutil_check_pending_actions(
  const enum EVSFSysUtilInterruptContext context, int retval, int fd)
{
  unsigned int i;
  /* Check the i/o handler before the signal handlers */
  if (s_io_handler && !s_io_handler_running && context == kVSFSysUtilIO)
  {
    s_io_handler_running = 1;
    (*s_io_handler)(retval, fd, s_p_io_handler_private);
    s_io_handler_running = 0;
  }
  for (i=0; i < NSIG; ++i)
  {
    if (s_sig_details[i].pending && !s_sig_details[i].running)
    {
      s_sig_details[i].running = 1;
      if (s_sig_details[i].sync_sig_handler)
      {
        s_sig_details[i].pending = 0;
        (*(s_sig_details[i].sync_sig_handler))(s_sig_details[i].p_private);
      }
      s_sig_details[i].running = 0;
    }
  }
}

static int
vsf_sysutil_translate_sig(const enum EVSFSysUtilSignal sig)
{
  int realsig = 0;
  switch (sig)
  {
    case kVSFSysUtilSigALRM:
      realsig = SIGALRM;
      break;
    case kVSFSysUtilSigTERM:
      realsig = SIGTERM;
      break;
    case kVSFSysUtilSigCHLD:
      realsig = SIGCHLD;
      break;
    case kVSFSysUtilSigPIPE:
      realsig = SIGPIPE;
      break;
    case kVSFSysUtilSigURG:
      realsig = SIGURG;
      break;
    case kVSFSysUtilSigHUP:
      realsig = SIGHUP;
      break;
    default:
      bug("unknown signal in vsf_sysutil_translate_sig");
      break;
  }
  if (realsig < 0 || realsig >= NSIG)
  {
    bug("signal out of range in vsf_sysutil_translate_sig");
  }
  return realsig;
}

void
vsf_sysutil_install_sighandler(const enum EVSFSysUtilSignal sig,
                               vsf_sighandle_t handler, void* p_private)
{
  int realsig = vsf_sysutil_translate_sig(sig);
  s_sig_details[realsig].p_private = p_private;
  s_sig_details[realsig].sync_sig_handler = handler;
  vsf_sysutil_set_sighandler(realsig, vsf_sysutil_common_sighandler);
}

void
vsf_sysutil_default_sig(const enum EVSFSysUtilSignal sig)
{
  int realsig = vsf_sysutil_translate_sig(sig);
  vsf_sysutil_set_sighandler(realsig, SIG_DFL);
  s_sig_details[realsig].p_private = NULL;
  s_sig_details[realsig].sync_sig_handler = NULL;
}

void
vsf_sysutil_install_null_sighandler(const enum EVSFSysUtilSignal sig)
{
  int realsig = vsf_sysutil_translate_sig(sig);
  s_sig_details[realsig].p_private = NULL;
  s_sig_details[realsig].sync_sig_handler = NULL;
  vsf_sysutil_set_sighandler(realsig, vsf_sysutil_common_sighandler);
}

void
vsf_sysutil_install_async_sighandler(const enum EVSFSysUtilSignal sig,
                                     vsf_async_sighandle_t handler)
{
  int realsig = vsf_sysutil_translate_sig(sig);
  s_sig_details[realsig].p_private = NULL;
  s_sig_details[realsig].sync_sig_handler = NULL;
  vsf_sysutil_set_sighandler(realsig, handler);
}

static void
vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
{
  int retval;
  struct sigaction sigact;
  vsf_sysutil_memclr(&sigact, sizeof(sigact));
  sigact.sa_handler = p_handlefunc;
  retval = sigemptyset(&sigact.sa_mask);
  if (retval != 0)
  {
    die("sigemptyset");
  }
  retval = sigaction(sig, &sigact, NULL);
  if (retval != 0)
  {
    die("sigaction");
  }
}

void
vsf_sysutil_install_io_handler(vsf_context_io_t handler, void* p_private)
{
  if (s_io_handler != NULL)
  {
    bug("double register of i/o handler");
  }
  s_io_handler = handler;
  s_p_io_handler_private = p_private;
}

void
vsf_sysutil_uninstall_io_handler(void)
{
  if (s_io_handler == NULL)
  {
    bug("no i/o handler to unregister!");
  }
  s_io_handler = NULL;
  s_p_io_handler_private = NULL;
}

void
vsf_sysutil_set_alarm(const unsigned int trigger_seconds)
{
  (void) alarm(trigger_seconds);
}

void
vsf_sysutil_clear_alarm(void)
{
  vsf_sysutil_set_alarm(0);
}

int
vsf_sysutil_read(const int fd, void* p_buf, const unsigned int size)
{
  while (1)
  {
    int retval = read(fd, p_buf, size);
    vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
    if (retval < 0 && errno == EINTR)
    {
      continue;
    }
    return retval;
  }
}

int
vsf_sysutil_write(const int fd, const void* p_buf, const unsigned int size)
{
  while (1)
  {
    int retval = write(fd, p_buf, size);
    vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
    if (retval < 0 && errno == EINTR)
    {
      continue;
    }
    return retval;
  }
}

int
vsf_sysutil_read_loop(const int fd, void* p_buf, unsigned int size)
{
  int retval;
  int num_read = 0;
  if (size > INT_MAX)
  {
    die("size too big in vsf_sysutil_read_loop");
  }
  while (1)
  {
    retval = vsf_sysutil_read(fd, (char*)p_buf + num_read, size);
    if (retval < 0)
    {
      return retval;
    }
    else if (retval == 0)
    {
      /* Read all we're going to read.. */
      return num_read; 
    }
    if ((unsigned int) retval > size)
    {
      die("retval too big in vsf_sysutil_read_loop");
    }
    num_read += retval;
    size -= (unsigned int) retval;
    if (size == 0)
    {
      /* Hit the read target, cool. */
      return num_read;
    }
  }
}

int
vsf_sysutil_write_loop(const int fd, const void* p_buf, unsigned int size)
{
  int retval;
  int num_written = 0;
  if (size > INT_MAX)
  {
    die("size too big in vsf_sysutil_write_loop");
  }
  while (1)
  {
    retval = vsf_sysutil_write(fd, (const char*)p_buf + num_written, size);
    if (retval < 0)
    {
      /* Error */
      return retval;
    }
    else if (retval == 0)
    {
      /* Written all we're going to write.. */
      return num_written;
    }
    if ((unsigned int) retval > size)
    {
      die("retval too big in vsf_sysutil_read_loop");
    }
    num_written += retval;
    size -= (unsigned int) retval;
    if (size == 0)
    {
      /* Hit the write target, cool. */
      return num_written;
    }
  }
}

filesize_t
vsf_sysutil_get_file_offset(const int file_fd)
{
  filesize_t retval = lseek(file_fd, 0, SEEK_CUR);
  if (retval < 0)
  {
    die("lseek");
  }
  return retval;
}

void
vsf_sysutil_lseek_to(const int fd, filesize_t seek_pos)
{
  filesize_t retval;
  if (seek_pos < 0)
  {
    die("negative seek_pos in vsf_sysutil_lseek_to");
  }
  retval = lseek(fd, seek_pos, SEEK_SET);
  if (retval < 0)
  {
    die("lseek");
  }
}

void*
vsf_sysutil_malloc(unsigned int size)
{
  void* p_ret;
  /* Paranoia - what if we got an integer overflow/underflow? */
  if (size == 0 || size > INT_MAX)
  {
    bug("zero or big size in vsf_sysutil_malloc");
  }  
  p_ret = malloc(size);
  if (p_ret == NULL)
  {
    die("malloc");
  }
  return p_ret;
}

void*
vsf_sysutil_realloc(void* p_ptr, unsigned int size)
{
  void* p_ret;
  if (size == 0 || size > INT_MAX)
  {
    bug("zero or big size in vsf_sysutil_realloc");
  }
  p_ret = realloc(p_ptr, size);
  if (p_ret == NULL)
  {
    die("realloc");
  }
  return p_ret;
}

void
vsf_sysutil_free(void* p_ptr)
{
  if (p_ptr == NULL)
  {
    bug("vsf_sysutil_free got a null pointer");
  }
  free(p_ptr);
}

unsigned int
vsf_sysutil_getpid(void)
{
  if (s_current_pid == -1)
  {
    s_current_pid = getpid();
  }
  return (unsigned int) s_current_pid;
}

int
vsf_sysutil_fork(void)
{
  int retval = fork();
  if (retval < 0)
  {
    die("fork");
  }
  else if (retval == 0)
  {
    s_current_pid = -1;
  }
  return retval;
}

void
vsf_sysutil_exit(int exit_code)
{
  _exit(exit_code);
}

struct vsf_sysutil_wait_retval
vsf_sysutil_wait(void)
{
  struct vsf_sysutil_wait_retval retval;
  vsf_sysutil_memclr(&retval, sizeof(retval));
  while (1)
  {
    int sys_ret = wait(&retval.exit_status);
    if (sys_ret < 0 && errno == EINTR)
    {
      vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
      continue;
    }
    retval.syscall_retval = sys_ret;
    return retval;
  }
}

int
vsf_sysutil_wait_reap_one(void)
{
  int retval = waitpid(-1, NULL, WNOHANG);
  if (retval == 0 || (retval < 0 && errno == ECHILD))
  {
    /* No more children */
    return 0;
  }
  /* Got one */
  return 1;
}

int
vsf_sysutil_wait_get_retval(const struct vsf_sysutil_wait_retval* p_waitret)
{
  return p_waitret->syscall_retval;
}

int
vsf_sysutil_wait_exited_normally(
  const struct vsf_sysutil_wait_retval* p_waitret)
{
  return WIFEXITED(p_waitret->exit_status);
}

int
vsf_sysutil_wait_get_exitcode(const struct vsf_sysutil_wait_retval* p_waitret)
{
  if (!vsf_sysutil_wait_exited_normally(p_waitret))
  {
    bug("not a normal exit in vsf_sysutil_wait_get_exitcode");
  }
  return WEXITSTATUS(p_waitret->exit_status);
}

void
vsf_sysutil_activate_keepalive(int fd)
{
  int keepalive = 1;
  int retval = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                          sizeof(keepalive));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_activate_reuseaddr(int fd)
{
  int reuseaddr = 1;
  int retval = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                          sizeof(reuseaddr));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_set_nodelay(int fd)
{
  int nodelay = 1;
  int retval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                          sizeof(nodelay));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_activate_sigurg(int fd)
{
  int retval = fcntl(fd, F_SETOWN, getpid());
  if (retval != 0)
  {
    die("fcntl");
  }
}

void
vsf_sysutil_activate_oobinline(int fd)
{
  int oob_inline = 1;
  int retval = setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &oob_inline,
                          sizeof(oob_inline));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_set_iptos_throughput(int fd)
{
  int tos = IPTOS_THROUGHPUT;
  /* Ignore failure to set (maybe this IP stack demands privilege for this) */
  (void) setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
}

void
vsf_sysutil_activate_linger(int fd)
{
  int retval;
  struct linger the_linger;
  the_linger.l_onoff = 1;
  the_linger.l_linger = 5 * 60;
  retval = setsockopt(fd, SOL_SOCKET, SO_LINGER, &the_linger,
                      sizeof(the_linger));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_deactivate_linger(int fd)
{
  int retval;
  struct linger the_linger;
  the_linger.l_onoff = 0;
  the_linger.l_linger = 0;
  retval = setsockopt(fd, SOL_SOCKET, SO_LINGER, &the_linger,
                      sizeof(the_linger));
  if (retval != 0)
  {
    die("setsockopt");
  }
}

void
vsf_sysutil_activate_noblock(int fd)
{
  int retval;
  int curr_flags = fcntl(fd, F_GETFL);
  if (vsf_sysutil_retval_is_error(curr_flags))
  {
    die("fcntl");
  }
  curr_flags |= O_NONBLOCK;
  retval = fcntl(fd, F_SETFL, curr_flags);
  if (retval != 0)
  {
    die("fcntl");
  }
}

void
vsf_sysutil_deactivate_noblock(int fd)
{
  int retval;
  int curr_flags = fcntl(fd, F_GETFL);
  if (vsf_sysutil_retval_is_error(curr_flags))
  {
    die("fcntl");
  }
  curr_flags &= ~O_NONBLOCK;
  retval = fcntl(fd, F_SETFL, curr_flags);
  if (retval != 0)
  {
    die("fcntl");
  }
}

int
vsf_sysutil_recv_peek(const int fd, void* p_buf, unsigned int len)
{
  while (1)
  {
    int retval = recv(fd, p_buf, len, MSG_PEEK);
    vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
    if (retval < 0 && errno == EINTR)
    {
      /* Kitsune update point; hack to escape the blocking loop */
      if (update_point != NULL) {
			  kitsune_update(update_point);
      }      
      continue;
    }
    return retval;
  }
}

int
vsf_sysutil_atoi(const char* p_str)
{
  return atoi(p_str);
}

filesize_t
vsf_sysutil_a_to_filesize_t(const char* p_str)
{
  /* atoll() is C99 standard - may break build on older platforms */
  return atoll(p_str);
}

const char*
vsf_sysutil_ulong_to_str(unsigned long the_ulong)
{
  static char ulong_buf[32];
  (void) snprintf(ulong_buf, sizeof(ulong_buf), "%lu", the_ulong);
  return ulong_buf;
}

const char*
vsf_sysutil_filesize_t_to_str(filesize_t the_filesize)
{
  static char filesize_buf[32];
  if (sizeof(long) == 8)
  {
    /* Avoid using non-standard %ll if we can */
    (void) snprintf(filesize_buf, sizeof(filesize_buf), "%ld",
                    (long) the_filesize);
  }
  else
  {
    (void) snprintf(filesize_buf, sizeof(filesize_buf), "%lld", the_filesize);
  }
  return filesize_buf;
}

const char*
vsf_sysutil_double_to_str(double the_double)
{
  static char double_buf[32];
  (void) snprintf(double_buf, sizeof(double_buf), "%.2f", the_double);
  return double_buf;
}

const char*
vsf_sysutil_uint_to_octal(unsigned int the_uint)
{
  static char octal_buf[32];
  if (the_uint == 0)
  {
    octal_buf[0] = '0';
    octal_buf[1] = '\0';
  }
  else
  {
    (void) snprintf(octal_buf, sizeof(octal_buf), "0%o", the_uint);
  }
  return octal_buf;
}

unsigned int
vsf_sysutil_octal_to_uint(const char* p_str)
{
  /* NOTE - avoiding using sscanf() parser */
  unsigned int result = 0;
  int seen_non_zero_digit = 0;
  while (*p_str != '\0')
  {
    if (!isdigit(*p_str) || *p_str > '7')
    {
      break;
    }
    if (*p_str != '0')
    {
      seen_non_zero_digit = 1;
    }
    if (seen_non_zero_digit)
    {
      result <<= 3;
      result += ( *p_str - '0' );
    }
    p_str++;
  }
  return result;
}

int
vsf_sysutil_toupper(int the_char)
{
  return toupper(the_char);
}

int
vsf_sysutil_isspace(int the_char)
{
  return isspace(the_char);
}

int
vsf_sysutil_isprint(int the_char)
{
  /* From Solar - we know better than some libc's! Don't let any potential
   * control chars through
   */
  unsigned char uchar = (unsigned char) the_char;
  if (uchar <= 31)
  {
    return 0;
  }
  if (uchar == 177)
  {
    return 0;
  }
  if (uchar >= 128 && uchar <= 159)
  {
    return 0;
  }
  return isprint(the_char);
}

int
vsf_sysutil_isalnum(int the_char)
{
  return isalnum(the_char);
}

char*
vsf_sysutil_getcwd(char* p_dest, const unsigned int buf_size)
{
  char* p_retval = getcwd(p_dest, buf_size);
  p_dest[buf_size - 1] = '\0';
  return p_retval;
}

int
vsf_sysutil_mkdir(const char* p_dirname, const unsigned int mode)
{
  return mkdir(p_dirname, mode);
}

int
vsf_sysutil_rmdir(const char* p_dirname)
{
  return rmdir(p_dirname);
}

int
vsf_sysutil_chdir(const char* p_dirname)
{
  return chdir(p_dirname);
}

int
vsf_sysutil_rename(const char* p_from, const char* p_to)
{
  return rename(p_from, p_to);
}

struct vsf_sysutil_dir*
vsf_sysutil_opendir(const char* p_dirname)
{
  return (struct vsf_sysutil_dir*) opendir(p_dirname);
}

void
vsf_sysutil_closedir(struct vsf_sysutil_dir* p_dir)
{
  DIR* p_real_dir = (DIR*) p_dir;
  int retval = closedir(p_real_dir);
  if (retval != 0)
  {
    die("closedir");
  }
}

const char*
vsf_sysutil_next_dirent(struct vsf_sysutil_dir* p_dir)
{
  DIR* p_real_dir = (DIR*) p_dir;
  struct dirent* p_dirent = readdir(p_real_dir);
  if (p_dirent == NULL)
  {
    return NULL;
  }
  return p_dirent->d_name;
}

unsigned int
vsf_sysutil_strlen(const char* p_text)
{
  return strlen(p_text);
}

char*
vsf_sysutil_strdup(const char* p_str)
{
  return strdup(p_str);
}

void
vsf_sysutil_memclr(void* p_dest, unsigned int size)
{
  /* Safety */
  if (size == 0)
  {
    return;
  }
  memset(p_dest, '\0', size);
}

void
vsf_sysutil_memcpy(void* p_dest, const void* p_src, const unsigned int size)
{
  /* Safety */
  if (size == 0)
  {
    return;
  }
  memcpy(p_dest, p_src, size);
}

int
vsf_sysutil_memcmp(const void* p_src1, const void* p_src2, unsigned int size)
{
  /* Safety */
  if (size == 0)
  {
    return 0;
  }
  return memcmp(p_src1, p_src2, size);
}

int
vsf_sysutil_strcmp(const char* p_src1, const char* p_src2)
{
  return strcmp(p_src1, p_src2);
}

unsigned int
vsf_sysutil_getpagesize(void)
{
  static unsigned int s_page_size;
  if (s_page_size == 0)
  {
    s_page_size = getpagesize();
    if (s_page_size == 0)
    {
      die("getpagesize");
    }
  }
  return s_page_size;
}

static int
vsf_sysutil_translate_memprot(const enum EVSFSysUtilMapPermission perm)
{
  int retval = 0;
  switch (perm)
  {
    case kVSFSysUtilMapProtReadOnly:
      retval = PROT_READ;
      break;
    case kVSFSysUtilMapProtNone:
      retval = PROT_NONE;
      break;
    default:
      bug("bad value in vsf_sysutil_translate_memprot");
      break;
  }
  return retval;
}

void
vsf_sysutil_memprotect(void* p_addr, unsigned int len,
                       const enum EVSFSysUtilMapPermission perm)
{
  int prot = vsf_sysutil_translate_memprot(perm);
  int retval = mprotect(p_addr, len, prot);
  if (retval != 0)
  {
    die("mprotect");
  }
}

void
vsf_sysutil_memunmap(void* p_start, unsigned int length)
{
  int retval = munmap(p_start, length);
  if (retval != 0)
  {
    die("munmap");
  }
}

static int
vsf_sysutil_translate_openmode(const enum EVSFSysUtilOpenMode mode)
{
  int retval = 0;
  switch (mode)
  {
    case kVSFSysUtilOpenReadOnly:
      retval = O_RDONLY;
      break;
    case kVSFSysUtilOpenWriteOnly:
      retval = O_WRONLY;
      break;
    case kVSFSysUtilOpenReadWrite:
      retval = O_RDWR;
      break;
    default:
      bug("bad mode in vsf_sysutil_translate_openmode");
      break;
  }
  return retval;
}

int
vsf_sysutil_open_file(const char* p_filename,
                      const enum EVSFSysUtilOpenMode mode)
{
  return open(p_filename, vsf_sysutil_translate_openmode(mode) | O_NONBLOCK);
}

int
vsf_sysutil_create_file(const char* p_filename)
{
  /* NOTE! 0666 mode used so umask() decides end mode */
  return open(p_filename, O_CREAT | O_EXCL | O_WRONLY | O_APPEND, 0666);
}

int
vsf_sysutil_create_overwrite_file(const char* p_filename)
{
  return open(p_filename, O_CREAT | O_TRUNC | O_WRONLY |
                          O_APPEND | O_NONBLOCK, 0666);
}

int
vsf_sysutil_create_or_open_file(const char* p_filename, unsigned int mode)
{
  return open(p_filename, O_CREAT | O_WRONLY | O_APPEND | O_NONBLOCK, mode);
}

void
vsf_sysutil_dupfd2(int old_fd, int new_fd)
{
  int retval;
  if (old_fd == new_fd)
  {
    return;
  }
  retval = dup2(old_fd, new_fd);
  if (retval != new_fd)
  {
    die("dup2");
  }
}

void
vsf_sysutil_close(int fd)
{
  while (1)
  {
    int retval = close(fd);
    if (retval != 0)
    {
      if (errno == EINTR)
      {
        vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
        continue;
      }
      die("close");
    }
    return;
  }
}

int
vsf_sysutil_close_failok(int fd)
{
  return close(fd);
}

int
vsf_sysutil_unlink(const char* p_dead)
{
  return unlink(p_dead);
}

int
vsf_sysutil_write_access(const char* p_filename)
{
  int retval = access(p_filename, W_OK);
  return (retval == 0);
}

static void
vsf_sysutil_alloc_statbuf(struct vsf_sysutil_statbuf** p_ptr)
{
  if (*p_ptr == NULL)
  {
    *p_ptr = vsf_sysutil_malloc(sizeof(struct stat));
  }
}

void
vsf_sysutil_fstat(int fd, struct vsf_sysutil_statbuf** p_ptr)
{
  int retval;
  vsf_sysutil_alloc_statbuf(p_ptr);
  retval = fstat(fd, (struct stat*) (*p_ptr));
  if (retval != 0)
  {
    die("fstat");
  }
}

int
vsf_sysutil_stat(const char* p_name, struct vsf_sysutil_statbuf** p_ptr)
{
  vsf_sysutil_alloc_statbuf(p_ptr);
  return stat(p_name, (struct stat*) (*p_ptr));
}

int
vsf_sysutil_lstat(const char* p_name, struct vsf_sysutil_statbuf** p_ptr)
{
  vsf_sysutil_alloc_statbuf(p_ptr);
  return lstat(p_name, (struct stat*) (*p_ptr));
}

void
vsf_sysutil_dir_stat(const struct vsf_sysutil_dir* p_dir,
                     struct vsf_sysutil_statbuf** p_ptr)
{
  int fd = dirfd((DIR*) p_dir);
  vsf_sysutil_fstat(fd, p_ptr);
}

int
vsf_sysutil_statbuf_is_regfile(const struct vsf_sysutil_statbuf* p_stat)
{
  const struct stat* p_realstat = (const struct stat*) p_stat;
  return S_ISREG(p_realstat->st_mode);
}

int
vsf_sysutil_statbuf_is_symlink(const struct vsf_sysutil_statbuf* p_stat)
{
  const struct stat* p_realstat = (const struct stat*) p_stat;
  return S_ISLNK(p_realstat->st_mode);
}

int
vsf_sysutil_statbuf_is_socket(const struct vsf_sysutil_statbuf* p_stat)
{
  const struct stat* p_realstat = (const struct stat*) p_stat;
  return S_ISSOCK(p_realstat->st_mode);
}

int
vsf_sysutil_statbuf_is_dir(const struct vsf_sysutil_statbuf* p_stat)
{
  const struct stat* p_realstat = (const struct stat*) p_stat;
  return S_ISDIR(p_realstat->st_mode);
}

const char*
vsf_sysutil_statbuf_get_perms(const struct vsf_sysutil_statbuf* p_statbuf)
{
  static char perms[11];
  int i;
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  for (i=0; i<10; i++)
  {
    perms[i] = '-';
  }
  perms[0] = '?';
  switch (p_stat->st_mode & S_IFMT)
  {
    case S_IFREG: perms[0] = '-'; break;
    case S_IFDIR: perms[0] = 'd'; break;
    case S_IFLNK: perms[0] = 'l'; break;
    case S_IFIFO: perms[0] = 'p'; break;
    case S_IFSOCK: perms[0] = 's'; break;
    case S_IFCHR: perms[0] = 'c'; break;
    case S_IFBLK: perms[0] = 'b'; break;
  }
  if (p_stat->st_mode & S_IRUSR) perms[1] = 'r';
  if (p_stat->st_mode & S_IWUSR) perms[2] = 'w';
  if (p_stat->st_mode & S_IXUSR) perms[3] = 'x';
  if (p_stat->st_mode & S_IRGRP) perms[4] = 'r';
  if (p_stat->st_mode & S_IWGRP) perms[5] = 'w';
  if (p_stat->st_mode & S_IXGRP) perms[6] = 'x';
  if (p_stat->st_mode & S_IROTH) perms[7] = 'r';
  if (p_stat->st_mode & S_IWOTH) perms[8] = 'w';
  if (p_stat->st_mode & S_IXOTH) perms[9] = 'x';
  if (p_stat->st_mode & S_ISUID) perms[3] = (perms[3] == 'x') ? 's' : 'S';
  if (p_stat->st_mode & S_ISGID) perms[6] = (perms[6] == 'x') ? 's' : 'S';
  if (p_stat->st_mode & S_ISVTX) perms[9] = (perms[9] == 'x') ? 't' : 'T';
  perms[10] = '\0';
  return perms;
}

const char*
vsf_sysutil_statbuf_get_date(const struct vsf_sysutil_statbuf* p_statbuf,
                             int use_localtime)
{
  static char datebuf[64];
  int retval;
  struct tm* p_tm;
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  long local_time = vsf_sysutil_get_cached_time_sec();
  const char* p_date_format = "%b %d %H:%M";
  if (!use_localtime)
  {
    p_tm = gmtime(&p_stat->st_mtime);
  }
  else
  {
    p_tm = localtime(&p_stat->st_mtime);
  }
  /* Is this a future or 6 months old date? If so, we drop to year format */
  if (p_stat->st_mtime > local_time ||
      (local_time - p_stat->st_mtime) > 60*60*24*182)
  {
    p_date_format = "%b %d  %Y";
  }
  retval = strftime(datebuf, sizeof(datebuf), p_date_format, p_tm);
  datebuf[sizeof(datebuf)-1] = '\0';
  if (retval == 0)
  {
    die("strftime");
  }
  return datebuf;
}

const char*
vsf_sysutil_statbuf_get_numeric_date(
  const struct vsf_sysutil_statbuf* p_statbuf,
  int use_localtime)
{
  static char datebuf[15];
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  struct tm* p_tm;
  int retval;
  if (!use_localtime)
  {
    p_tm = gmtime(&p_stat->st_mtime);
  }
  else
  {
    p_tm = localtime(&p_stat->st_mtime);
  }
  retval = strftime(datebuf, sizeof(datebuf), "%Y%m%d%H%M%S", p_tm);
  if (retval == 0)
  {
    die("strftime");
  }
  return datebuf;
}

filesize_t
vsf_sysutil_statbuf_get_size(const struct vsf_sysutil_statbuf* p_statbuf)
{
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  if (p_stat->st_size < 0)
  {
    die("invalid inode size in vsf_sysutil_statbuf_get_size");
  }
  return p_stat->st_size;
}

int
vsf_sysutil_statbuf_get_uid(const struct vsf_sysutil_statbuf* p_statbuf)
{
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  return p_stat->st_uid;
}

int
vsf_sysutil_statbuf_get_gid(const struct vsf_sysutil_statbuf* p_statbuf)
{
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  return p_stat->st_gid;
}

unsigned int
vsf_sysutil_statbuf_get_links(const struct vsf_sysutil_statbuf* p_statbuf)
{
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  return p_stat->st_nlink;
}

int
vsf_sysutil_statbuf_is_readable_other(
  const struct vsf_sysutil_statbuf* p_statbuf)
{
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  if (p_stat->st_mode & S_IROTH)
  {
    return 1;
  }
  return 0;
}

const char*
vsf_sysutil_statbuf_get_sortkey_mtime(
  const struct vsf_sysutil_statbuf* p_statbuf)
{
  static char intbuf[32];
  const struct stat* p_stat = (const struct stat*) p_statbuf;
  /* This slight hack function must return a character date format such that
   * more recent dates appear later in the alphabet! Most notably, we must
   * make sure we pad to the same length with 0's 
   */
  snprintf(intbuf, sizeof(intbuf), "%030ld", p_stat->st_mtime);
  return intbuf;
}

void
vsf_sysutil_fchown(const int fd, const int uid, const int gid)
{
  if (fchown(fd, uid, gid) != 0)
  {
    die("fchown");
  }
}

int
vsf_sysutil_chmod(const char* p_filename, unsigned int mode)
{
  /* Safety: mask "mode" to just access permissions, e.g. no suid setting! */
  mode = mode & 0777;
  return chmod(p_filename, mode);
}

int
vsf_sysutil_lock_file(int fd)
{
  struct flock the_lock;
  int retval;
  vsf_sysutil_memclr(&the_lock, sizeof(the_lock));
  the_lock.l_type = F_WRLCK;
  the_lock.l_whence = SEEK_SET;
  the_lock.l_start = 0;
  the_lock.l_len = 0;
  do
  {
    retval = fcntl(fd, F_SETLKW, &the_lock);
    vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
  }
  while (retval < 0 && errno == EINTR);
  return retval;
}

void
vsf_sysutil_unlock_file(int fd)
{
  int retval;
  struct flock the_lock;
  vsf_sysutil_memclr(&the_lock, sizeof(the_lock));
  the_lock.l_type = F_UNLCK;
  the_lock.l_whence = SEEK_SET;
  the_lock.l_start = 0;
  the_lock.l_len = 0;
  retval = fcntl(fd, F_SETLK, &the_lock);
  if (retval != 0)
  {
    die("fcntl");
  }
}

int
vsf_sysutil_readlink(const char* p_filename, char* p_dest, unsigned int bufsiz)
{
  int retval = readlink(p_filename, p_dest, bufsiz - 1);
  if (retval < 0)
  {
    return retval;
  }
  /* Ensure buffer is NULL terminated; readlink(2) doesn't do that */
  p_dest[retval] = '\0';
  return retval;
}

int
vsf_sysutil_retval_is_error(int retval)
{
  if (retval < 0)
  {
    return 1;
  }
  return 0;
}

enum EVSFSysUtilError
vsf_sysutil_get_error(void)
{
  enum EVSFSysUtilError retval = kVSFSysUtilErrUnknown;
  switch (errno)
  {
    case EADDRINUSE:
      retval = kVSFSysUtilErrADDRINUSE;
      break;
    case ENOSYS:
      retval = kVSFSysUtilErrNOSYS;
      break;
    case EINTR:
      retval = kVSFSysUtilErrINTR;
      break;
    case EINVAL:
      retval = kVSFSysUtilErrINVAL;
      break;
  }
  return retval;
}

int
vsf_sysutil_get_ipv4_sock(void)
{
  int retval = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (retval < 0)
  {
    die("socket");
  }
  return retval;
}

struct vsf_sysutil_socketpair_retval
vsf_sysutil_unix_dgram_socketpair(void)
{
  struct vsf_sysutil_socketpair_retval retval;
  int the_sockets[2];
  int sys_retval = socketpair(PF_UNIX, SOCK_DGRAM, 0, the_sockets);
  if (sys_retval != 0)
  {
    die("socketpair");
  }
  retval.socket_one = the_sockets[0];
  retval.socket_two = the_sockets[1];
  return retval;
}

int
vsf_sysutil_bind(int fd, const struct vsf_sysutil_sockaddr* p_sockptr)
{
  struct sockaddr* p_sockaddr = (struct sockaddr*) p_sockptr;
  return bind(fd, p_sockaddr, sizeof(struct sockaddr_in));
}

void
vsf_sysutil_listen(int fd, const unsigned int backlog)
{
  int retval = listen(fd, backlog);
  if (retval != 0)
  {
    die("listen");
  }
}

int
vsf_sysutil_accept_timeout(int fd, struct vsf_sysutil_sockaddr** p_sockptr,
                           unsigned int wait_seconds)
{
  struct sockaddr_in remote_addr;
  int retval;
  fd_set accept_fdset;
  struct timeval timeout;
  unsigned int socklen = sizeof(remote_addr);
  if (p_sockptr)
  {
    vsf_sysutil_sockaddr_clear(p_sockptr);
  }
  if (wait_seconds > 0)
  {
    FD_ZERO(&accept_fdset);
    FD_SET(fd, &accept_fdset);
    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do
    {
      retval = select(fd + 1, &accept_fdset, NULL, NULL, &timeout);
      vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
    } while (retval < 0 && errno == EINTR);
    if (retval == 0)
    {
      errno = EAGAIN;
      return -1;
    }
  }
  retval = accept(fd, (struct sockaddr*) &remote_addr, &socklen);
  if (retval < 0)
  {
    return retval;
  }
  /* FreeBSD bug / paranoia: ai32@drexel.edu */
  if (socklen == 0)
  {
    return -1;
  }
  if (remote_addr.sin_family != AF_INET)
  {
    die("can only support ipv4 currently");
  }
  if (p_sockptr)
  {
    *p_sockptr = vsf_sysutil_malloc(sizeof(remote_addr));
    vsf_sysutil_memcpy(*p_sockptr, &remote_addr, sizeof(remote_addr));
  }
  return retval;
}

int
vsf_sysutil_connect_timeout(int fd, const struct vsf_sysutil_sockaddr* p_addr,
                            unsigned int wait_seconds)
{
  const struct sockaddr_in* p_sockaddr = (const struct sockaddr_in*) p_addr;
  unsigned int addrlen = sizeof(*p_sockaddr);
  int retval;
  if (wait_seconds > 0)
  {
    vsf_sysutil_activate_noblock(fd);
  }
  retval = connect(fd, (const struct sockaddr*)p_sockaddr, addrlen);
  if (retval < 0 && errno == EINPROGRESS)
  {
    fd_set connect_fdset;
    struct timeval timeout;
    FD_ZERO(&connect_fdset);
    FD_SET(fd, &connect_fdset);
    timeout.tv_sec = wait_seconds;
    timeout.tv_usec = 0;
    do
    {
      retval = select(fd + 1, NULL, &connect_fdset, NULL, &timeout);
      vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
    }
    while (retval < 0 && errno == EINTR);
    if (retval == 0)
    {
      retval = -1;
      errno = EAGAIN;
    }
    else
    {
      int socklen = sizeof(retval);
      int sockoptret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &retval, &socklen);
      if (sockoptret != 0)
      {
        die("getsockopt");
      }
    }
  }
  if (wait_seconds > 0)
  {
    vsf_sysutil_deactivate_noblock(fd);
  }
  return retval;
}

void
vsf_sysutil_getsockname(int fd, struct vsf_sysutil_sockaddr** p_sockptr)
{
  struct sockaddr_in the_addr;
  int retval;
  unsigned int socklen = sizeof(the_addr);
  vsf_sysutil_sockaddr_clear(p_sockptr);
  retval = getsockname(fd, (struct sockaddr*) &the_addr, &socklen);
  if (retval != 0)
  {
    die("getsockname");
  }
  if (the_addr.sin_family != AF_INET)
  {
    die("can only support ipv4 currently");
  }
  *p_sockptr = vsf_sysutil_malloc(sizeof(the_addr));
  vsf_sysutil_memcpy(*p_sockptr, &the_addr, sizeof(the_addr));
}

void
vsf_sysutil_getpeername(int fd, struct vsf_sysutil_sockaddr** p_sockptr)
{
  struct sockaddr_in the_addr;
  int retval;
  unsigned int socklen = sizeof(the_addr);
  vsf_sysutil_sockaddr_clear(p_sockptr);
  retval = getpeername(fd, (struct sockaddr*) &the_addr, &socklen);
  if (retval != 0)
  {
    die("getpeername");
  }
  if (the_addr.sin_family != AF_INET)
  {
    die("can only support ipv4 currently");
  }
  *p_sockptr = vsf_sysutil_malloc(sizeof(the_addr));
  vsf_sysutil_memcpy(*p_sockptr, &the_addr, sizeof(the_addr));
}

void
vsf_sysutil_shutdown_failok(int fd)
{
  /* SHUT_RDWR is a relatively new addition */
  #ifndef SHUT_RDWR
  #define SHUT_RDWR 2
  #endif
  (void) shutdown(fd, SHUT_RDWR);
}

void
vsf_sysutil_sockaddr_clear(struct vsf_sysutil_sockaddr** p_sockptr)
{
  if (*p_sockptr != NULL)
  {
    vsf_sysutil_free(*p_sockptr);
    *p_sockptr = NULL;
  }
}

void
vsf_sysutil_sockaddr_alloc_ipv4(struct vsf_sysutil_sockaddr** p_sockptr)
{
  struct sockaddr_in new_addr;
  vsf_sysutil_sockaddr_clear(p_sockptr);
  *p_sockptr = vsf_sysutil_malloc(sizeof(new_addr));
  vsf_sysutil_memclr(&new_addr, sizeof(new_addr));
  new_addr.sin_family = AF_INET;
  vsf_sysutil_memcpy(*p_sockptr, &new_addr, sizeof(new_addr));
}

void
vsf_sysutil_sockaddr_set_ipaddr(struct vsf_sysutil_sockaddr* p_sockptr,
                                struct vsf_sysutil_ipv4addr the_addr)
{
  struct sockaddr_in* p_sockaddr = (struct sockaddr_in*) p_sockptr;
  vsf_sysutil_memcpy(&p_sockaddr->sin_addr.s_addr, the_addr.data,
                     sizeof(p_sockaddr->sin_addr.s_addr));
}

struct vsf_sysutil_ipv4addr
vsf_sysutil_sockaddr_get_ipaddr(const struct vsf_sysutil_sockaddr* p_sockptr)
{
  struct vsf_sysutil_ipv4addr retval;
  const struct sockaddr_in* p_sockaddr = (const struct sockaddr_in*) p_sockptr;
  vsf_sysutil_memcpy(retval.data, &p_sockaddr->sin_addr.s_addr,
                     sizeof(retval.data));
  return retval;
}

struct vsf_sysutil_ipv4addr
vsf_sysutil_sockaddr_get_any(void)
{
  struct vsf_sysutil_ipv4addr retval;
  vsf_sysutil_memclr(&retval, sizeof(retval));
  return retval;
}

struct vsf_sysutil_ipv4port
vsf_sysutil_sockaddr_get_port(const struct vsf_sysutil_sockaddr* p_sockptr)
{
  struct vsf_sysutil_ipv4port retval;
  const struct sockaddr_in* p_sockaddr = (const struct sockaddr_in*) p_sockptr;
  vsf_sysutil_memcpy(retval.data, &p_sockaddr->sin_port, sizeof(retval.data));
  return retval;
}

struct vsf_sysutil_ipv4port
vsf_sysutil_ipv4port_from_int(unsigned int port)
{
  struct vsf_sysutil_ipv4port retval;
  unsigned short netorder_port = htons(port);
  vsf_sysutil_memcpy(retval.data, &netorder_port, sizeof(retval.data));
  return retval;
}

void
vsf_sysutil_sockaddr_set_port(struct vsf_sysutil_sockaddr* p_sockptr,
                              struct vsf_sysutil_ipv4port the_port)
{
  struct sockaddr_in* p_sockaddrin = (struct sockaddr_in*) p_sockptr;
  vsf_sysutil_memcpy(&p_sockaddrin->sin_port, the_port.data,
                     sizeof(p_sockaddrin->sin_port));
}

int
vsf_sysutil_is_port_reserved(const struct vsf_sysutil_ipv4port the_port)
{
  unsigned short netorder_port;
  vsf_sysutil_memcpy(&netorder_port, the_port.data, sizeof(netorder_port));
  if (ntohs(netorder_port) < IPPORT_RESERVED)
  {
    return 1;
  }
  return 0;
}

const char*
vsf_sysutil_inet_ntoa(const struct vsf_sysutil_sockaddr* p_sockptr)
{
  const struct sockaddr_in* p_sockaddr = (const struct sockaddr_in*) p_sockptr;
  return inet_ntoa(p_sockaddr->sin_addr);
}

int
vsf_sysutil_inet_aton(const char* p_text, struct vsf_sysutil_ipv4addr* p_addr)
{
  struct in_addr sin_addr;
  if (inet_aton(p_text, &sin_addr))
  {
    vsf_sysutil_memcpy(p_addr, &sin_addr.s_addr, sizeof(*p_addr));
    return 1;
  }
  else
  {
    return 0;
  }
}

struct vsf_sysutil_user*
vsf_sysutil_getpwuid(const int uid)
{
  if (uid < 0)
  {
    bug("negative uid in vsf_sysutil_getpwuid");
  }
  return (struct vsf_sysutil_user*) getpwuid((unsigned int) uid);
}

struct vsf_sysutil_user*
vsf_sysutil_getpwnam(const char* p_user)
{
  return (struct vsf_sysutil_user*) getpwnam(p_user);
}

const char*
vsf_sysutil_user_getname(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  return p_passwd->pw_name;
}

const char*
vsf_sysutil_user_get_homedir(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  return p_passwd->pw_dir;
}

int
vsf_sysutil_user_getuid(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  return p_passwd->pw_uid;
}

int
vsf_sysutil_user_getgid(const struct vsf_sysutil_user* p_user)
{ 
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  return p_passwd->pw_gid;
}

struct vsf_sysutil_group*
vsf_sysutil_getgrgid(const int gid)
{
  if (gid < 0)
  {
    die("negative gid in vsf_sysutil_getgrgid");
  }
  return (struct vsf_sysutil_group*) getgrgid((unsigned int) gid);
}

const char*
vsf_sysutil_group_getname(const struct vsf_sysutil_group* p_group)
{
  const struct group* p_grp = (const struct group*) p_group;
  return p_grp->gr_name;
}

unsigned char
vsf_sysutil_get_random_byte(void)
{
  static int seeded;
  unsigned int uint_res;
  unsigned char c1, c2, c3, c4;
  if (!seeded)
  {
    struct timeval tv;
    int retval = gettimeofday(&tv, NULL);
    if (retval != 0)
    {
      die("gettimeofday");
    }
    srand((unsigned)tv.tv_usec);
    seeded = 1;
  }
  uint_res = rand();
  c1 = uint_res & 0x000000ff;
  c2 = (uint_res >> 8) & 0x000000ff;
  c3 = (uint_res >> 16) & 0x000000ff;
  c4 = (uint_res >> 24) & 0x000000ff;
  return c1 ^ c2 ^ c3 ^ c4;    
}

int
vsf_sysutil_running_as_root(void)
{
  return (getuid() == 0);
}

void
vsf_sysutil_setuid(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  vsf_sysutil_setuid_numeric(p_passwd->pw_uid);
}

void
vsf_sysutil_setuid_numeric(int uid)
{
  int retval = setuid(uid);
  if (retval != 0)
  {
    die("setuid");
  }
}

void
vsf_sysutil_setgid(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  vsf_sysutil_setgid_numeric(p_passwd->pw_gid);
}

void
vsf_sysutil_setgid_numeric(int gid)
{
  int retval = setgid(gid);
  if (retval != 0)
  {
    die("setgid");
  }
}

int
vsf_sysutil_geteuid(void)
{
  int retval = geteuid();
  if (retval < 0)
  {
    die("geteuid");
  }
  return retval;
}

int
vsf_sysutil_getegid(void)
{
  int retval = getegid();
  if (retval < 0)
  {
    die("getegid");
  }
  return retval;
}

void
vsf_sysutil_seteuid(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  vsf_sysutil_seteuid_numeric(p_passwd->pw_uid);
}

void
vsf_sysutil_setegid(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  vsf_sysutil_setegid_numeric(p_passwd->pw_gid);
}

void
vsf_sysutil_seteuid_numeric(int uid)
{
  /* setreuid() would seem to be more portable than seteuid() */
  int retval = setreuid(-1, uid);
  if (retval != 0)
  {
    die("seteuid");
  }
}

void
vsf_sysutil_setegid_numeric(int gid)
{
  /* setregid() would seem to be more portable than setegid() */
  int retval = setregid(-1, gid);
  if (retval != 0)
  {
    die("setegid");
  }
}

void
vsf_sysutil_clear_supp_groups(void)
{
  int retval = setgroups(0, NULL);
  if (retval != 0)
  {
    die("setgroups");
  }
}

void
vsf_sysutil_initgroups(const struct vsf_sysutil_user* p_user)
{
  const struct passwd* p_passwd = (const struct passwd*) p_user;
  int retval = initgroups(p_passwd->pw_name, p_passwd->pw_gid);
  if (retval != 0)
  {
    die("initgroups");
  }
}

void
vsf_sysutil_chroot(const char* p_root_path)
{
  int retval = chroot(p_root_path);
  if (retval != 0)
  {
    die("chroot");
  }
}

unsigned int
vsf_sysutil_get_umask(void)
{
  return s_current_umask;
}

void
vsf_sysutil_set_umask(unsigned int new_umask)
{
  s_current_umask = (new_umask & 0777);
  (void) umask(s_current_umask);
}

void
vsf_sysutil_make_session_leader(void)
{
  /* This makes us the leader if we are not already */
  (void) setsid();
  /* Check we're the leader */
  if (getpid() != getpgrp())
  {
    die("not session leader");
  }
}

void
vsf_sysutil_tzset(void)
{
  tzset();
}

const char*
vsf_sysutil_get_current_date(void)
{
  static char datebuf[64];
  time_t curr_time;
  const struct tm* p_tm;
  vsf_sysutil_update_cached_time();
  curr_time = vsf_sysutil_get_cached_time_sec();
  p_tm = localtime(&curr_time);
  if (strftime(datebuf, sizeof(datebuf), "%a %b %d %H:%M:%S %Y", p_tm) == 0)
  {
    die("strftime");
  }
  datebuf[sizeof(datebuf) - 1] = '\0';
  return datebuf;
}

void
vsf_sysutil_update_cached_time(void)
{
  if (gettimeofday(&s_current_time, NULL) != 0)
  {
    die("gettimeofday");
  }
}

long
vsf_sysutil_get_cached_time_sec(void)
{
  return s_current_time.tv_sec;
}

long
vsf_sysutil_get_cached_time_usec(void)
{
  return s_current_time.tv_usec;
}

void
vsf_sysutil_qsort(void* p_base, unsigned int num_elem, unsigned int elem_size,
                  int (*p_compar)(const void *, const void *))
{
  qsort(p_base, num_elem, elem_size, p_compar);
}

void
vsf_sysutil_sleep(double seconds)
{
  int retval;
  double fractional;
  time_t secs;
  struct timespec ts;
  secs = (time_t) seconds;
  fractional = seconds - (double) secs;
  ts.tv_sec = secs;
  ts.tv_nsec = (long) (fractional * (double) 1000000000);
  do
  {
    retval = nanosleep(&ts, &ts);
    vsf_sysutil_check_pending_actions(kVSFSysUtilUnknown, 0, 0);
  } while (retval == -1 && errno == EINTR);
}
