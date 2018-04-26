#ifndef VSF_FTPCMDIO_H
#define VSF_FTPCMDIO_H

struct mystr;
struct vsf_session;

/* vsf_cmdio_sock_setup()
 * PURPOSE
 * Initialise a few socket settings (keepalive, nonagle, etc). on the FTP
 * control connection.
 */
void vsf_cmdio_sock_setup(void);

/* vsf_cmdio_write()
 * PURPOSE
 * Write a response to the FTP control connection.
 * PARAMETERS
 * p_sess       - the current session object
 * status       - the status code to report
 * p_text       - the text to report
 */
void vsf_cmdio_write(struct vsf_session* p_sess, int status,
                     const char* p_text);

/* vsf_cmdio_write_noblock()
 * PURPOSE
 * The same as vsf_cmdio_write(), apart from the fact we _guarantee_ not to
 * block (ditching output if neccessary). This is useful for messages as
 * we exit, to avoid getting stuck on exit.
 */
void vsf_cmdio_write_noblock(struct vsf_session* p_sess, int status,
                             const char* p_text);

/* vsf_cmdio_write_str()
 * PURPOSE
 * The same as vsf_cmdio_write(), apart from the text is specified as a
 * string buffer object "p_str".
 */
void vsf_cmdio_write_str(struct vsf_session* p_sess, int status,
                         const struct mystr* p_str);

/* vsf_cmdio_write_str_hyphen()
 * PURPOSE
 * The same as vsf_cmdio_write_str(), apart from the response line is
 * output with the continuation indicator '-' between the response code and
 * the response text. This indicates there are more lines of response.
 */
void vsf_cmdio_write_str_hyphen(struct vsf_session* p_sess, int status,
                                const struct mystr* p_str);

/* vsf_cmdio_set_alarm()
 * PURPOSE
 * Activate the control connection inactivity timeout. This is explicitly
 * exposed in the API so that we can play it safe, and activate the alarm
 * before _any_ potentially blocking calls.
 * PARAMETERS
 * p_sess       - The current session object
 */
void vsf_cmdio_set_alarm(struct vsf_session* p_sess);

/* vsf_cmdio_get_cmd_and_arg()
 * PURPOSE
 * Read an FTP command (and optional argument) from the FTP control connection.
 * PARAMETERS
 * p_sess       - The current session object
 * p_cmd_str    - Where to put the FTP command string (may be empty)
 * p_arg_str    - Where to put the FTP argument string (may be empty)
 * set_alarm    - If true, the control connection inactivity monitor is used
 */
void vsf_cmdio_get_cmd_and_arg(struct vsf_session* p_sess,
                               struct mystr* p_cmd_str,
                               struct mystr* p_arg_str, int set_alarm);

#endif /* VSF_FTPCMDIO_H */

