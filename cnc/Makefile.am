pkginclude_HEADERS = cnc.h
pkginclude_internaldir = $(pkgincludedir)/internal
pkginclude_internal_HEADERS = internal/context.h \
							  internal/item_collection.h \
							  internal/step_collection.h \
							  internal/step_launcher.h \
							  internal/tag_collection.h

uninstall-hook:
	-test -d "$(pkgincludedir)" && $(RM) $(pkgincludedir)
