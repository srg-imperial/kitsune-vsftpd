# Makefile for systems with GNU tools

LASTVERSION=1.1.2
VERSION=1.1.3

# KITSUNE
ifdef KITSUNE_PATH
KITSUNE_INCLUDE = -I $(KITSUNE_PATH)/include
KITSUNE_LIB = -L $(KITSUNE_PATH)/lib
KITSUNE_BIN = $(KITSUNE_PATH)/bin/
else
KITSUNE_INCLUDE =
KITSUNE_LIB =
KITSUNE_BIN =
endif
EKINC  = $(KITSUNE_INCLUDE)
EKLIB = $(KITSUNE_LIB) -lkitsune
CFLAGS_SHARED  = -ggdb3 -Wall -ldl -shared -fPIC -u kitsune_init_inplace
CFLAGS = -ggdb3 -Wall -ldl -fPIC $(EKINC) $(EKLIB)
EKDRV = $(KITSUNE_BIN)driver
EKCC = $(KITSUNE_BIN)ktcc --dostackvars --automigrate --doglobalreg --keepunused -include kitsune.h
EKJOIN = $(KITSUNE_BIN)kttjoin
EKGEN = $(KITSUNE_BIN)xfgen

USER_XFORM=vsftpd.xf
GENERATED_XFORM=dsu.c

IFLAGS  = -idirafter dummyinc

LIBS	=	`./vsf_findlibs.sh` -lcrypt
LINK	=	

OBJS	=	main.o utility.o prelogin.o ftpcmdio.o postlogin.o privsock.o \
		tunables.o ftpdataio.o secbuf.o ls.o \
		postprivparent.o logging.o str.o netstr.o sysstr.o strlist.o \
    banner.o filestr.o parseconf.o secutil.o \
    ascii.o oneprocess.o twoprocess.o privops.o standalone.o hash.o \
    tcpwrap.o \
    sysutil.o sysdeputil.o

all: vsftpd

.c.o:
	$(EKCC) $(CFLAGS) $(IFLAGS) -c $*.c --doktsavetypes --typesfile-out=$*.ktt

vsftpd-$(VERSION).ktt: $(OBJS)
	$(EKJOIN) $@ $(patsubst %.o,%.ktt,$(OBJS))

$(GENERATED_XFORM): vsftpd-$(VERSION).ktt $(USER_XFORM)
	$(EKGEN) $@ ../vsftpd-$(LASTVERSION)/vsftpd-$(LASTVERSION).ktt vsftpd-$(VERSION).ktt $(USER_XFORM)

vsftpd: $(OBJS) $(GENERATED_XFORM)
	$(CC) $(CFLAGS_SHARED) -ggdb3 -o vsftpd.so $(OBJS) $(GENERATED_XFORM) $(EKINC) $(EKLIB) $(LINK) $(LIBS)
	cp vsftpd.so vsftpd2.so

clean:
	rm -f *.o *.swp vsftpd*.so *.cil.c *.cil.i *.i *.ktt dsu.c

