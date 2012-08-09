SUBDIRS = cnc samples

.PHONY: cleansamples testsamples testall

cleansamples:
	$(MAKE) clean -C samples

testsamples: check

testall: check

DISTCHECK_CONFIGURE_FLAGS = @EXTRA_DISTCHECK_OPTIONS@
