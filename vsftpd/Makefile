SUBDIRS = vsftpd-*.* orig_vsftpd/vsftpd-*.* ek_vsftpd_1pt

.PHONY: subdirs $(SUBDIRS)
     
subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean ); done
