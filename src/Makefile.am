libexec_PROGRAMS = tracker-miner-media

tracker_miner_media_SOURCES =	\
	tracker-miner-media.c	\
	tracker-miner-media.h	\
	main.c

tracker_miner_media_CPPFLAGS =	\
    -DG_LOG_DOMAIN=\"Tmm\"	\
    -I$(top_srcdir)/src		\
    $(DEPS_CFLAGS)

tracker_miner_media_LDADD =	\
    $(DEPS_LIBS)
