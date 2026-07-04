SUBDIRS := draw font server wm widgets desktop

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for directory in $(SUBDIRS); do $(MAKE) -C $$directory clean; done
