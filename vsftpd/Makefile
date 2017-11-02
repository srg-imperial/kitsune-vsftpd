#SUBDIRS = vsftpd-*.* orig_vsftpd/vsftpd-*.* ek_vsftpd_1pt
SUBDIRS = vsftpd-*.* orig_vsftpd/vsftpd-*.*

.PHONY: subdirs $(SUBDIRS)
     
subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean ); done
