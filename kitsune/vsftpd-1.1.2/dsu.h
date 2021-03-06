#ifndef   	DSU_H_
# define   	DSU_H_

#ifndef VSFTP_STR_H
#include "str.h"
#endif

#ifndef VSF_FILESIZE_H
#include "filesize.h"
#endif

/* Session struct from 1.1.1 to allow typecasting */
struct vsf_session_old
{
  /* Details of the control connection */
  struct vsf_sysutil_sockaddr* p_local_addr;
  struct vsf_sysutil_sockaddr* p_remote_addr;

  /* Details of the data connection */
  int pasv_listen_fd;
  struct vsf_sysutil_sockaddr* p_port_sockaddr;
  int data_fd;
  int data_progress;
  unsigned int bw_rate_max;
  long bw_send_start_sec;
  long bw_send_start_usec;

  /* Details of the login */
  int is_anonymous;
  struct mystr user_str;
  struct mystr anon_pass_str;

  /* Details of the FTP protocol state */
  filesize_t restart_pos;
  int is_ascii;
  struct mystr rnfr_filename_str;
  int abor_received;

  /* Details of FTP session state */
  struct mystr_list* p_visited_dir_list;

  /* Details of userids which are interesting to us */
  int anon_ftp_uid;
  int anon_upload_chown_uid;

  /* Things we need to cache before we chroot() */
  struct mystr banned_email_str;
  struct mystr userlist_str;
  struct mystr banner_str;

  /* Logging related details */
  int log_fd;
  struct mystr remote_ip_str;
  unsigned long log_type;
  long log_start_sec;
  long log_start_usec;
  struct mystr log_str;
  filesize_t transfer_size;

  /* Buffers */
  struct mystr ftp_cmd_str;
  struct mystr ftp_arg_str;

  /* Parent<->child comms channel */
  int privsock_inited;
  int parent_fd;
  int child_fd;

  /* Other details */
  int num_clients;
};

#endif 	    /* !DSU_H_ */
