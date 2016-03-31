.PHONY: clean default test

OPENSSL   = $(shell brew --prefix openssl)
CC        = clang
CFLAGS    = -O3 -I$(OPENSSL)/include
LDFLAGS   =
LIBCRYPTO = $(OPENSSL)/lib/libcrypto.a

default: railroll-generate-manifest

railroll-generate-manifest: $(LIBCRYPTO) generate-manifest.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f railroll-generate-manifest *.o

test: railroll-generate-manifest
	./$< test/smallfile > /tmp/railroll-test-output
	diff test/smallfile.manifest /tmp/railroll-test-output

