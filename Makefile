# Common Makefile for mini-os.
#
# Every architecture directory below mini-os/arch has to have a
# Makefile and a arch.mk.
#

OBJ_DIR=$(CURDIR)
TOPLEVEL_DIR=$(CURDIR)

ifeq ($(MINIOS_CONFIG),)
include Config.mk
else
EXTRA_DEPS += $(MINIOS_CONFIG)
include $(MINIOS_CONFIG)
endif

include $(MINI-OS_ROOT)/config/MiniOS.mk

MINIOS_REV=$(shell date --rfc-3339=sec)
flags-y += "-DMINIOS_REV=\"mini-er $(MINIOS_REV)\""

DEF_CFLAGS += $(flags-y)

# Symlinks and headers that must be created before building the C files
GENERATED_HEADERS := include/list.h $(ARCH_LINKS) include/mini-os $(TARGET_ARCH_DIR)/include/mini-os

ifeq ($(MINIOS_TARGET_ARCH),arm32)
GENERATED_HEADERS += include/fdt.h include/libfdt.h
endif

EXTRA_DEPS += $(GENERATED_HEADERS)

include/%.h: dtc/libfdt/%.h
	ln -s ../$^ $@

# Installation location
PREFIX ?= /usr
LIBDIR ?= ${PREFIX}/lib
INCLUDEDIR ?= ${PREFIX}/include

# Include common mini-os makerules.
include minios.mk

# Define some default flags for linking.
LDLIBS := 
APP_LDLIBS := 
LDARCHLIB := -L$(OBJ_DIR)/$(TARGET_ARCH_DIR) -l$(ARCH_LIB_NAME)
LDFLAGS_FINAL := -T $(TARGET_ARCH_DIR)/minios-$(MINIOS_TARGET_ARCH).lds

# Prefix for global API names. All other symbols are localised before
# linking with EXTRA_OBJS.
GLOBAL_PREFIX := xenos_
EXTRA_OBJS =

TARGET := mini-os

# Subdirectories common to mini-os
SUBDIRS := lib console dtc/libfdt

FDT_SRC :=
ifeq ($(MINIOS_TARGET_ARCH),arm32)
# Need libgcc.a for division helpers
LDLIBS += `$(CC) -print-libgcc-file-name`

# Device tree support
FDT_SRC := dtc/libfdt/fdt.c dtc/libfdt/fdt_ro.c dtc/libfdt/fdt_strerror.c

src-y += ${FDT_SRC}
endif

src-y += events.c
src-y += gntmap.c
src-y += gnttab.c
src-y += hypervisor.c
src-y += kernel.c
src-y += mm.c

src-y += lib/ctype.c
ifneq ($(MINIOS_TARGET_ARCH),arm32)
src-y += lib/math.c
endif
src-y += lib/printf.c
src-y += lib/string.c
src-y += lib/memmove.c
src-y += lib/assert.c
src-y += lib/xmalloc.c

src-y += console/console.c
src-y += console/xencons_ring.c

# The common mini-os objects to build.
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(src-y))

.PHONY: default
default: libminios.a

# Create special architecture specific links. The function arch_links
# has to be defined in arch.mk (see include above).
ifneq ($(ARCH_LINKS),)
$(ARCH_LINKS):
	$(arch_links)
endif

include/list.h: include/minios-external/bsd-sys-queue-h-seddery include/minios-external/bsd-sys-queue.h
	perl $^ --prefix=minios  >$@.new
	$(call move-if-changed,$@.new,$@)

# Used by stubdom's Makefile
.PHONY: links
links: $(GENERATED_HEADERS)

include/mini-os:
	ln -sf . $@

$(TARGET_ARCH_DIR)/include/mini-os:
	ln -sf . $@

.PHONY: arch_lib
arch_lib:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) || exit 1;

$(OBJ_DIR)/$(TARGET)_app.o: app.lds
	$(LD) -r -d $(LDFLAGS) -\( $^ -\) $(APP_LDLIBS) --undefined main -o $@

libminios.a: $(HEAD_OBJ) $(APP_O) $(OBJS) arch_lib
	rm -f $@
	ar rcs $@ $(APP_O) $(OBJS) $(OBJ_DIR)/$(TARGET_ARCH_DIR)/*.o

%.pc: %.pc.in
	sed \
	  -e 's/@ARCH_LDFLAGS@/${ARCH_LDFLAGS}/g' \
	  -e 's/@ARCH_CFLAGS@/${ARCH_CFLAGS}/g' \
	  -e 's!@GCC_INSTALL@!${GCC_INSTALL}!g' \
	  $^ > $@

# Note: don't install directly to $(DESTDIR)$(LIBDIR) because pkg-config strips out directories in the
# standard search path but we usually compile using -nostdlib.
.PHONY: install
install: libminios.a libminios-xen.pc
	$(INSTALL_DIR) $(DESTDIR)$(LIBDIR)/minios-xen
	$(INSTALL_DIR) $(DESTDIR)$(LIBDIR)/pkgconfig
	$(INSTALL_DIR) $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DIR) $(DESTDIR)$(INCLUDEDIR)/minios-xen
	$(eval REALDESTDIR := $(abspath $(DESTDIR)))
	$(INSTALL_DATA) libminios.a $(REALDESTDIR)$(LIBDIR)/minios-xen
	$(INSTALL_DATA) $(OBJ_DIR)/$(TARGET_ARCH_DIR)/$(ARCH_LIB) $(REALDESTDIR)$(LIBDIR)/minios-xen
	$(INSTALL_DATA) arch/$(TARGET_ARCH_FAM)/minios-$(MINIOS_TARGET_ARCH).lds $(REALDESTDIR)$(LIBDIR)/minios-xen/libminios.lds
	(cd include && find -H . -type d | xargs -I {} $(INSTALL_DIR) $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/{})
	(cd include && find -H . -type f | xargs -I {} $(INSTALL_DATA) {} $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/{})
	(cd include/xen && find -H . -type d | xargs -I {} $(INSTALL_DIR) $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/xen/{})
	(cd include/xen && find -H . -type f | xargs -I {} $(INSTALL_DATA) {} $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/xen/{})
	[ -L $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/mini-os ] || ln -s . $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/mini-os
	$(INSTALL_DATA) $(foreach dir,$(EXTRA_INC),$(wildcard $(MINI-OS_ROOT)/$(dir)/*.h)) $(REALDESTDIR)$(INCLUDEDIR)/minios-xen/
	$(INSTALL_DATA) libminios-xen.pc $(REALDESTDIR)$(LIBDIR)/pkgconfig

.PHONY: clean arch_clean

arch_clean:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) clean || exit 1;

clean:	arch_clean
	for dir in $(addprefix $(OBJ_DIR)/,$(SUBDIRS)); do \
		rm -f $$dir/*.o; \
	done
	rm -f include/list.h
	rm -f $(OBJ_DIR)/*.o *~ $(OBJ_DIR)/core $(OBJ_DIR)/$(TARGET).elf $(OBJ_DIR)/$(TARGET).raw $(OBJ_DIR)/$(TARGET) $(OBJ_DIR)/$(TARGET).gz
	find . $(OBJ_DIR) -type l | xargs rm -f
	rm -f tags TAGS
	rm -f libminios-xen.pc
	rm -f libminios.a

