/*
 * Part of Very Secure FTPd
 * Licence: GPL
 * Author: Chris Evans
 * prelogin.c
 *
 * Code to parse the FTP protocol prior to a successful login.
 */

#include "prelogin.h"
#include "ftpcmdio.h"
#include "ftpcodes.h"
#include "str.h"
#include "vsftpver.h"
#include "tunables.h"
#include "oneprocess.h"
#include "twoprocess.h"
#include "sysdeputil.h"
#include "sysutil.h"
#include "session.h"
#include "banner.h"
#include "logging.h"

/* Functions used */
static void emit_greeting(struct vsf_session* p_sess);
static void parse_username_password(struct vsf_session* p_sess);
static void handle_user_command(struct vsf_session* p_sess);
static void handle_pass_command(struct vsf_session* p_sess);

void
init_connection(struct vsf_session* p_sess)
{
  if (tunable_setproctitle_enable)
  {
    vsf_sysutil_setproctitle("not logged in");
  }
  /* Before we talk to the remote, make sure an alarm is set up in case
   * writing the initial greetings should block.
   */
  vsf_cmdio_set_alarm(p_sess);
/* Kitsune, prevent printing greeting if just updated */  
  if (!kitsune_is_updating()) {  
    emit_greeting(p_sess);
  }
  parse_username_password(p_sess);
}

static void
emit_greeting(struct vsf_session* p_sess)
{
  struct mystr str_log_line = INIT_MYSTR;
  /* Check for client limits (standalone mode only) */
  if (tunable_max_clients > 0 &&
      p_sess->num_clients > tunable_max_clients)
  {
    str_alloc_text(&str_log_line, "Connection refused: too many sessions.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_noblock(p_sess, FTP_TOO_MANY_USERS,
                    "There are too many connected users, please try later.");
    vsf_sysutil_exit(0);
  }
  if (tunable_max_per_ip > 0 &&
      p_sess->num_this_ip > tunable_max_per_ip)
  {
    str_alloc_text(&str_log_line,
                   "Connection refused: too many sessions for this address.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_noblock(p_sess, FTP_IP_LIMIT,
        "There are too many connections from your internet address.");
    vsf_sysutil_exit(0);
  }
  if (!p_sess->tcp_wrapper_ok)
  {
    str_alloc_text(&str_log_line,
                   "Connection refused: tcp_wrappers denial.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_noblock(p_sess, FTP_IP_DENY, "Service not available.");
    vsf_sysutil_exit(0);
  }
  vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
  if (!str_isempty(&p_sess->banner_str))
  {
    vsf_banner_write(p_sess, &p_sess->banner_str, FTP_GREET);
    str_free(&p_sess->banner_str);
    vsf_cmdio_write(p_sess, FTP_GREET, "");
  }
  else if (tunable_ftpd_banner == 0)
  {
    vsf_cmdio_write(p_sess, FTP_GREET, "(vsFTPd " VSF_VERSION 
                    ")");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_GREET, tunable_ftpd_banner);
  }
}

static void
parse_username_password(struct vsf_session* p_sess)
{
  while (1)
  {
    /* Kitsune: update point */
		kitsune_update("prelogin.c");
    /* Kitsune */  
    vsf_sysutil_kitsune_set_update_point("prelogin.c");
    
    vsf_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
                              &p_sess->ftp_arg_str, 1);
    if (str_equal_text(&p_sess->ftp_cmd_str, "USER"))
    {
      handle_user_command(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "PASS"))
    {
      handle_pass_command(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "QUIT"))
    {
      vsf_cmdio_write(p_sess, FTP_GOODBYE, "Goodbye.");
      vsf_sysutil_exit(0);
    }
    else
    {
      vsf_cmdio_write(p_sess, FTP_LOGINERR,
                      "Please login with USER and PASS.");
    }
  }
}

static void
handle_user_command(struct vsf_session* p_sess)
{
  /* SECURITY: If we're in anonymous only-mode, immediately reject
   * non-anonymous usernames in the hope we save passwords going plaintext
   * over the network
   */
  int is_anon = 1;
  str_copy(&p_sess->user_str, &p_sess->ftp_arg_str);
  str_upper(&p_sess->ftp_arg_str);
  if (!str_equal_text(&p_sess->ftp_arg_str, "FTP") &&
      !str_equal_text(&p_sess->ftp_arg_str, "ANONYMOUS"))
  {
    is_anon = 0;
  }
  if (!tunable_local_enable && !is_anon)
  {
    vsf_cmdio_write(p_sess, FTP_LOGINERR,
                    "This FTP server is anonymous only.");
    str_empty(&p_sess->user_str);
    return;
  }
  if (!str_isempty(&p_sess->userlist_str))
  {
    int located = str_contains_line(&p_sess->userlist_str, &p_sess->user_str);
    if ((located && tunable_userlist_deny) ||
        (!located && !tunable_userlist_deny))
    {
      vsf_cmdio_write(p_sess, FTP_LOGINERR, "Permission denied.");
      str_empty(&p_sess->user_str);
      return;
    }
  }
  if (is_anon && tunable_no_anon_password)
  {
    /* Fake a password */
    str_alloc_text(&p_sess->ftp_arg_str, "<no password>");
    handle_pass_command(p_sess);
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_GIVEPWORD, "Please specify the password.");
  }
}

static void
handle_pass_command(struct vsf_session* p_sess)
{
  if (str_isempty(&p_sess->user_str))
  {
    vsf_cmdio_write(p_sess, FTP_NEEDUSER, "Login with USER first.");
    return;
  }
  /* These login calls never return if successful */
  if (tunable_one_process_model)
  {
    vsf_one_process_login(p_sess, &p_sess->ftp_arg_str);
  }
  else
  {
    vsf_two_process_login(p_sess, &p_sess->ftp_arg_str);
  }
  vsf_cmdio_write(p_sess, FTP_LOGINERR, "Login incorrect.");
  str_empty(&p_sess->user_str);
  /* FALLTHRU if login fails */
}

