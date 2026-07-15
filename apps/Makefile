ROOT := ..
SUBDIRS := browser calculator file_manager image_viewer terminal text_editor

.PHONY: all clean libraries $(SUBDIRS)

all: libraries $(SUBDIRS)

libraries:
	$(MAKE) -C $(ROOT)/userland/libc
	$(MAKE) -C $(ROOT)/libnv0

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for directory in $(SUBDIRS); do $(MAKE) -C $$directory clean; done
