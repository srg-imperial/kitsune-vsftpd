time (cd ../../ && make -j16 -B && cd - \
	&& \
	for i in 1.1.3 1.2.0 1.2.1 1.2.2; \
		do pushd vsftpd-$i; make -B -j16; popd; \
	done)

