/** Inplace Kitsune update code by Michail Denchev <mdenchev@gmail.com, 
 * Jun 2011
 * Old Kitsune update code was done by Edward Smith <teddy@stormbringer>,
 * Jun 2010
 *
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define PRIVATE_HANDS_OFF_p_buf p_buf
#define PRIVATE_HANDS_OFF_len len
#define PRIVATE_HANDS_OFF_alloc_bytes alloc_bytes
#include "str.h"
#define PRIVATE_HANDS_OFF_alloc_len alloc_len
#define PRIVATE_HANDS_OFF_list_len list_len
#define PRIVATE_HANDS_OFF_p_nodes p_nodes
#include "strlist.h"

#include "standalone.h"
#include "session.h"
#include "hash.h"
#include "sysutil.h"
#include "tunables.h"
#include "dsu.h"

struct mystr_list_node
{
  struct mystr str;
  struct mystr sort_key_str;
};

int LOCAL_XFORM(main, the_session)(void *session) {
	struct vsf_session_old *old_session = (struct vsf_session_old *) stackvars_get_local("main", "the_session");
	assert(old_session);
	struct vsf_session *new_session = (struct vsf_session *) session; 

/* copy control connection */
	if (old_session->p_local_addr != NULL) {
		new_session->p_local_addr = malloc(sizeof(struct sockaddr_in));
		memcpy(new_session->p_local_addr, old_session->p_local_addr, sizeof(struct sockaddr_in)); 
		free(old_session->p_local_addr);
	}

	if (old_session->p_remote_addr != NULL) {
		new_session->p_remote_addr = malloc(sizeof(struct sockaddr_in)); 
		memcpy(new_session->p_remote_addr, old_session->p_remote_addr, sizeof(struct sockaddr_in));
		free(old_session->p_remote_addr);
	}

	/* copy data connection */
	new_session->pasv_listen_fd = old_session->pasv_listen_fd;
	if (old_session->p_port_sockaddr != NULL) {
		new_session->p_port_sockaddr = malloc(sizeof(struct sockaddr_in));
		memcpy(new_session->p_port_sockaddr, old_session->p_port_sockaddr, sizeof(struct sockaddr_in));
		free(old_session->p_port_sockaddr);
	}
	new_session->data_fd = old_session->data_fd;
	new_session->data_progress = old_session->data_progress;
	new_session->bw_rate_max = old_session->bw_rate_max;
	new_session->bw_send_start_sec = old_session->bw_send_start_sec;
	new_session->bw_send_start_usec = old_session->bw_send_start_usec;

	/* copy login details */
	new_session->is_anonymous = old_session->is_anonymous;
	str_copy(&(new_session->user_str), &(old_session->user_str));
	str_copy(&(new_session->anon_pass_str), &(old_session->anon_pass_str));
	
	/* copy ftp protocol state */
	new_session->restart_pos = old_session->restart_pos;
	new_session->is_ascii = old_session->is_ascii;
	str_copy(&(new_session->rnfr_filename_str), &(old_session->rnfr_filename_str));
	new_session->abor_received = old_session->abor_received;

	/* copy ftp session state */
	struct mystr_list the_list = INIT_STRLIST;
  new_session->p_visited_dir_list = vsf_sysutil_malloc(sizeof(struct mystr_list));
  *new_session->p_visited_dir_list = the_list;

	struct mystr_list *mystr_l = old_session->p_visited_dir_list;
	if (mystr_l != NULL) {
		int num_nodes = mystr_l->list_len;
		new_session->p_visited_dir_list->list_len = 0;
		new_session->p_visited_dir_list->alloc_len = mystr_l->alloc_len; /* save some reallocs */
		new_session->p_visited_dir_list->p_nodes = (void*) 0;
		int i;	
		for (i = 0; i < num_nodes; i++) {
			str_list_add(new_session->p_visited_dir_list, &(mystr_l->p_nodes[i].str),
					&(mystr_l->p_nodes[i].sort_key_str));
		}
	}

	/* copy userids */
	new_session->anon_ftp_uid = old_session->anon_ftp_uid;
	new_session->anon_upload_chown_uid;
		/* guest_user_uid added; main should init it properly */

	/* copy cache */
	str_copy(&(new_session->banned_email_str), &(old_session->banned_email_str));
	str_copy(&(new_session->userlist_str), &(old_session->userlist_str));
	str_copy(&(new_session->banner_str), &(old_session->banner_str));
	new_session->tcp_wrapper_ok = old_session->tcp_wrapper_ok;

	/* copy logging related details */
		/* let main + logging.c handle init of xferlog_fd and log_fd */
	str_copy(&(new_session->remote_ip_str), &(old_session->remote_ip_str));
	new_session->log_type = old_session->log_type;	
	new_session->log_start_sec = old_session->log_start_sec;
	new_session->log_start_usec = old_session->log_start_usec;
	str_copy(&(new_session->log_str), &(old_session->log_str));
	new_session->transfer_size = old_session->transfer_size;

	/* copy buffers */
	str_copy(&(new_session->ftp_cmd_str), &(old_session->ftp_cmd_str));
	str_copy(&(new_session->ftp_arg_str), &(old_session->ftp_arg_str));

	/* copy parent<->child comms channel */	
	new_session->parent_fd = old_session->parent_fd;
	new_session->child_fd = old_session->child_fd;

	/* copy other details */
	new_session->num_clients = old_session->num_clients;
	new_session->num_this_ip = old_session->num_this_ip;

	return 1;
}

