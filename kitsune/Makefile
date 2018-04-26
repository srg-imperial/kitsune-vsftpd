SUBDIRS = vsftpd-*.*

.PHONY: subdirs $(SUBDIRS)
     
subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean ); done
