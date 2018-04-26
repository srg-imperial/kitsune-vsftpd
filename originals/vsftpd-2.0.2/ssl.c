/*
 * Part of Very Secure FTPd
 * Licence: GPL v2. Note that this code interfaces with with the OpenSSL
 * libraries, so please read LICENSE where I give explicit permission to link
 * against the OpenSSL libraries.
 * Author: Chris Evans
 * ssl.c
 *
 * Routines to handle a SSL/TLS-based implementation of RFC 2228, i.e.
 * encryption.
 */

#include "ssl.h"
#include "session.h"
#include "ftpcodes.h"
#include "ftpcmdio.h"
#include "defs.h"
#include "str.h"
#include "sysutil.h"
#include "tunables.h"
#include "utility.h"
#include "builddefs.h"

#ifdef VSF_BUILD_SSL
  #include <openssl/ssl.h>
#endif

#ifdef VSF_BUILD_SSL

#include <openssl/err.h>
#include <openssl/rand.h>

static char* get_ssl_error();
static SSL* get_ssl(struct vsf_session* p_sess, int fd);
static int ssl_session_init(struct vsf_session* p_sess);

static int ssl_inited;

void
ssl_init(struct vsf_session* p_sess)
{
  if (!ssl_inited)
  {
    SSL_CTX* p_ctx;
    long options;
    SSL_library_init();
    p_ctx = SSL_CTX_new(SSLv23_server_method());
    if (p_ctx == NULL)
    {
      die("SSL: could not allocate SSL context");
    }
    options = SSL_OP_ALL;
    if (!tunable_sslv2)
    {
      options |= SSL_OP_NO_SSLv2;
    }
    if (!tunable_sslv3)
    {
      options |= SSL_OP_NO_SSLv3;
    }
    if (!tunable_tlsv1)
    {
      options |= SSL_OP_NO_TLSv1;
    }
    SSL_CTX_set_options(p_ctx, options);
    if (tunable_rsa_cert_file)
    {
      if (SSL_CTX_use_certificate_file(
        p_ctx, tunable_rsa_cert_file, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load RSA key");
      }
      if (SSL_CTX_use_PrivateKey_file(
        p_ctx, tunable_rsa_cert_file, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load RSA key");
      }
    }
    if (tunable_dsa_cert_file)
    {
      if (SSL_CTX_use_certificate_file(
        p_ctx, tunable_dsa_cert_file, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load DSA key");
      }
      if (SSL_CTX_use_PrivateKey_file(
        p_ctx, tunable_dsa_cert_file, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load DSA key");
      }
    }
    if (SSL_CTX_set_cipher_list(p_ctx, tunable_ssl_ciphers) != 1)
    {
      die("SSL: could not set cipher list");
    }
    if (RAND_status() != 1)
    {
      die("SSL: RNG is not seeded");
    }
    p_sess->p_ssl_ctx = p_ctx;
    ssl_inited = 1;
  }
}

void
handle_auth(struct vsf_session* p_sess)
{
  str_upper(&p_sess->ftp_arg_str);
  if (str_equal_text(&p_sess->ftp_arg_str, "TLS") ||
      str_equal_text(&p_sess->ftp_arg_str, "TLS-C") ||
      str_equal_text(&p_sess->ftp_arg_str, "SSL") ||
      str_equal_text(&p_sess->ftp_arg_str, "TLS-P"))
  {
    vsf_cmdio_write(p_sess, FTP_AUTHOK, "Proceed with negotiation.");
    if (!ssl_session_init(p_sess))
    {
      struct mystr err_str = INIT_MYSTR;
      str_alloc_text(&err_str, "Negotiation failed: ");
      str_append_text(&err_str, get_ssl_error());
      vsf_cmdio_write_str(p_sess, FTP_TLS_FAIL, &err_str);
      vsf_sysutil_exit(0);
    }
    p_sess->control_use_ssl = 1;
    if (str_equal_text(&p_sess->ftp_arg_str, "SSL") ||
        str_equal_text(&p_sess->ftp_arg_str, "TLS-P"))
    {
      p_sess->data_use_ssl = 1;
    }
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADAUTH, "Unknown AUTH type.");
  }
}

void
handle_pbsz(struct vsf_session* p_sess)
{
  if (!p_sess->control_use_ssl)
  {
    vsf_cmdio_write(p_sess, FTP_BADPBSZ, "PBSZ needs a secure connection.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_PBSZOK, "PBSZ set to 0.");
  }
}

void
handle_prot(struct vsf_session* p_sess)
{
  str_upper(&p_sess->ftp_arg_str);
  if (!p_sess->control_use_ssl)
  {
    vsf_cmdio_write(p_sess, FTP_BADPROT, "PROT needs a secure connection.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "C"))
  {
    p_sess->data_use_ssl = 0;
    vsf_cmdio_write(p_sess, FTP_PROTOK, "PROT now Clear.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "P"))
  {
    p_sess->data_use_ssl = 1;
    vsf_cmdio_write(p_sess, FTP_PROTOK, "PROT now Private.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "S") ||
           str_equal_text(&p_sess->ftp_arg_str, "E"))
  {
    vsf_cmdio_write(p_sess, FTP_NOHANDLEPROT, "PROT not supported.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_NOSUCHPROT, "PROT not recognized.");
  }
}

void
ssl_getline(const struct vsf_session* p_sess, struct mystr* p_str,
            char end_char, char* p_buf, unsigned int buflen)
{
  char* p_buf_start = p_buf;
  p_buf[buflen - 1] = '\0';
  buflen--;
  while (1)
  {
    int retval = SSL_read(p_sess->p_control_ssl, p_buf, buflen);
    if (retval <= 0)
    {
      die("SSL_read");
    }
    p_buf[retval] = '\0';
    buflen -= retval;
    if (p_buf[retval - 1] == end_char || buflen == 0)
    {
      break;
    }
    p_buf += retval;
  }
  str_alloc_alt_term(p_str, p_buf_start, end_char);
}

int
ssl_read(void* p_ssl, char* p_buf, unsigned int len)
{
  int retval = SSL_read((SSL*) p_ssl, p_buf, len);
  int fd = SSL_get_fd((SSL*) p_ssl);
  vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
  return retval;
}

int
ssl_write(void* p_ssl, const char* p_buf, unsigned int len)
{
  int retval = SSL_write((SSL*) p_ssl, p_buf, len);
  int fd = SSL_get_fd((SSL*) p_ssl);
  vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
  return retval;
}

int
ssl_write_str(void* p_ssl, const struct mystr* p_str)
{
  unsigned int len = str_getlen(p_str);
  int ret = SSL_write((SSL*) p_ssl, str_getbuf(p_str), len);
  if ((unsigned int) ret != len)
  {
    return -1;
  }
  return 0;
}

int
ssl_accept(struct vsf_session* p_sess, int fd)
{
  SSL* p_ssl = get_ssl(p_sess, fd);
  if (p_ssl == NULL)
  {
    return 0;
  }
  p_sess->p_data_ssl = p_ssl;
  return 1;
}

void
ssl_data_close(struct vsf_session* p_sess)
{
  SSL_free(p_sess->p_data_ssl);
}

void
ssl_comm_channel_init(struct vsf_session* p_sess)
{
  const struct vsf_sysutil_socketpair_retval retval =
    vsf_sysutil_unix_stream_socketpair();
  p_sess->ssl_consumer_fd = retval.socket_one;
  p_sess->ssl_slave_fd = retval.socket_two;
}

static SSL*
get_ssl(struct vsf_session* p_sess, int fd)
{
  SSL* p_ssl = SSL_new(p_sess->p_ssl_ctx);
  if (p_ssl == NULL)
  {
    return NULL;
  }
  if (!SSL_set_fd(p_ssl, fd))
  {
    SSL_free(p_ssl);
    return NULL;
  }
  if (SSL_accept(p_ssl) != 1)
  {
    die(get_ssl_error());
    SSL_free(p_ssl);
    return NULL;
  }
  return p_ssl;
}

static int
ssl_session_init(struct vsf_session* p_sess)
{
  SSL* p_ssl = get_ssl(p_sess, VSFTP_COMMAND_FD);
  if (p_ssl == NULL)
  {
    return 0;
  }
  p_sess->p_control_ssl = p_ssl;
  return 1;
}

static char*
get_ssl_error()
{
  SSL_load_error_strings();
  return ERR_error_string(ERR_get_error(), NULL);
}

#else /* VSF_BUILD_SSL */

void
ssl_init(struct vsf_session* p_sess)
{
  (void) p_sess;
  die("SSL: ssl_enable is set but SSL support not compiled in");
}

void
handle_auth(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
handle_pbsz(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
handle_prot(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
ssl_getline(const struct vsf_session* p_sess, struct mystr* p_str,
            char end_char, char* p_buf, unsigned int buflen)
{
  (void) p_sess;
  (void) p_str;
  (void) end_char;
  (void) p_buf;
  (void) buflen;
}

int
ssl_read(void* p_ssl, char* p_buf, unsigned int len)
{
  (void) p_ssl;
  (void) p_buf;
  (void) len;
  return -1;
}

int
ssl_write(void* p_ssl, const char* p_buf, unsigned int len)
{
  (void) p_ssl;
  (void) p_buf;
  (void) len;
  return -1;
}

int
ssl_write_str(void* p_ssl, const struct mystr* p_str)
{
  (void) p_ssl;
  (void) p_str;
  return -1;
}

int
ssl_accept(struct vsf_session* p_sess, int fd)
{
  (void) p_sess;
  (void) fd;
  return -1;
}

void
ssl_data_close(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
ssl_comm_channel_init(struct vsf_session* p_sess)
{
  (void) p_sess;
}

#endif /* VSF_BUILD_SSL */

