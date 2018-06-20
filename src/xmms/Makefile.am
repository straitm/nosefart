#SUBDIRS=nosefart
lib_LTLIBRARIES = libnosefart.la
libnosefart_la_LDFLAGS = -module -avoid-version
#libnosefart_la_LIBADD =\
# nosefart/libnosefart.la

libnosefart_la_SOURCES =\
 nosefart_xmms.c\
 ../log.c\
 ../memguard.c\
 ../cpu/nes6502/nes6502.c\
 ../cpu/nes6502/dis6502.c\
 ../machine/nsf.c\
 ../sndhrdw/nes_apu.c\
 ../sndhrdw/vrcvisnd.c\
 ../sndhrdw/fmopl.c\
 ../sndhrdw/vrc7_snd.c\
 ../sndhrdw/mmc5_snd.c\
 ../sndhrdw/fds_snd.c

INCLUDES = -DNSF_PLAYER=1 @XMMS_CFLAGS@ \
 -I..\
 -I../sndhrdw\
 -I../machine\
 -I../cpu/nes6502


install-exec-local:
	@for i in "$(DESTDIR)$(libdir)/libnosefart."*; \
		do $(LN_S) -fv "$$i" "$(DESTDIR)@XMMS_INPUT_PLUGIN_DIR@"; \
	done;
