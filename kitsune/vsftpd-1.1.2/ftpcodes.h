#ifndef VSF_FTPCODES_H
#define VSF_FTPCODES_H

#define FTP_DATACONN          150

#define FTP_NOOPOK            200
#define FTP_TYPEOK            200
#define FTP_PORTOK            200
#define FTP_UMASKOK           200
#define FTP_CHMODOK           200
#define FTP_SIZEOK            213
#define FTP_MDTMOK            213
#define FTP_SYSTOK            215
#define FTP_GREET             220
#define FTP_GOODBYE           221
#define FTP_ABOR_NOCONN       225
#define FTP_TRANSFEROK        226
#define FTP_ABOROK            226
#define FTP_PASVOK            227
#define FTP_LOGINOK           230
#define FTP_CWDOK             250
#define FTP_RMDIROK           250
#define FTP_DELEOK            250
#define FTP_RENAMEOK          250
#define FTP_PWDOK             257
#define FTP_MKDIROK           257

#define FTP_GIVEPWORD         331
#define FTP_RESTOK            350
#define FTP_RNFROK            350

#define FTP_IDLE_TIMEOUT      421
#define FTP_DATA_TIMEOUT      421
#define FTP_TOO_MANY_USERS    421
#define FTP_IP_LIMIT          421
#define FTP_BADSENDCONN       425
#define FTP_BADSENDNET        426
#define FTP_BADSENDFILE       451

#define FTP_BADCMD            500
#define FTP_BADHELP           502
#define FTP_NEEDUSER          503
#define FTP_NEEDRNFR          503
#define FTP_LOGINERR          530
#define FTP_FILEFAIL          550
#define FTP_NOPERM            550
#define FTP_UPLOADFAIL        553

#endif /* VSF_FTPCODES_H */
