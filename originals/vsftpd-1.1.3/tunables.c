/*
 * Part of Very Secure FTPd
 * Licence: GPL
 * Author: Chris Evans
 * tunables.c
 */

#include "tunables.h"

int tunable_anonymous_enable = 1;
int tunable_local_enable = 0;
int tunable_pasv_enable = 1;
int tunable_port_enable = 1;
int tunable_chroot_local_user = 0;
int tunable_write_enable = 0;
int tunable_anon_upload_enable = 0;
int tunable_anon_mkdir_write_enable = 0;
int tunable_anon_other_write_enable = 0;
int tunable_chown_uploads = 0;
int tunable_connect_from_port_20 = 0;
int tunable_xferlog_enable = 0;
int tunable_dirmessage_enable = 0;
int tunable_anon_world_readable_only = 1;
int tunable_async_abor_enable = 0;
int tunable_ascii_upload_enable = 0;
int tunable_ascii_download_enable = 0;
int tunable_one_process_model = 0;
int tunable_xferlog_std_format = 0;
int tunable_pasv_promiscuous = 0;
int tunable_deny_email_enable = 0;
int tunable_chroot_list_enable = 0;
int tunable_setproctitle_enable = 0;
int tunable_text_userdb_names = 0;
int tunable_ls_recurse_enable = 0;
int tunable_log_ftp_protocol = 0;
int tunable_guest_enable = 0;
int tunable_userlist_enable = 0;
int tunable_userlist_deny = 1;
int tunable_use_localtime = 0;
int tunable_check_shell = 1;
int tunable_hide_ids = 0;
int tunable_listen = 0;
int tunable_port_promiscuous = 0;
int tunable_passwd_chroot_enable = 0;
int tunable_no_anon_password = 0;
int tunable_tcp_wrappers = 0;
int tunable_use_sendfile = 1;

unsigned int tunable_accept_timeout = 60;
unsigned int tunable_connect_timeout = 60;
unsigned int tunable_local_umask = 077;
unsigned int tunable_anon_umask = 077;
unsigned int tunable_ftp_data_port = 20;
unsigned int tunable_idle_session_timeout = 300;
unsigned int tunable_data_connection_timeout = 300;
/* IPPORT_USERRESERVED + 1 */
unsigned int tunable_pasv_min_port = 5001;
unsigned int tunable_pasv_max_port = 0;
unsigned int tunable_anon_max_rate = 0;
unsigned int tunable_local_max_rate = 0;
/* IPPORT_FTP */
unsigned int tunable_listen_port = 21;
unsigned int tunable_max_clients = 0;
/* -rw-rw-rw- */
unsigned int tunable_file_open_mode = 0666;
unsigned int tunable_max_per_ip = 0;

const char* tunable_secure_chroot_dir = "/usr/share/empty";
const char* tunable_ftp_username = "ftp";
const char* tunable_chown_username = "root";
const char* tunable_xferlog_file = "/var/log/vsftpd.log";
const char* tunable_message_file = ".message";
/* XXX -> "secure"? */
const char* tunable_nopriv_user = "nobody";
const char* tunable_ftpd_banner = 0;
const char* tunable_banned_email_file = "/etc/vsftpd.banned_emails";
const char* tunable_chroot_list_file = "/etc/vsftpd.chroot_list";
const char* tunable_pam_service_name = "ftp";
const char* tunable_guest_username = "ftp";
const char* tunable_userlist_file = "/etc/vsftpd.user_list";
const char* tunable_anon_root = 0;
const char* tunable_local_root = 0;
const char* tunable_banner_file = 0;
const char* tunable_pasv_address = 0;
const char* tunable_listen_address = 0;
const char* tunable_user_config_dir = 0;

