#Don't change this unless needed, else you'll break stuff

bin_PROGRAMS = toxic


toxic_SOURCES = $(top_srcdir)/src/main.c \
                $(top_srcdir)/src/chat.h \
                $(top_srcdir)/src/chat.c \
                $(top_srcdir)/src/configdir.h \
                $(top_srcdir)/src/configdir.c \
                $(top_srcdir)/src/prompt.h \
                $(top_srcdir)/src/prompt.c \
                $(top_srcdir)/src/friendlist.h \
                $(top_srcdir)/src/friendlist.c \
                $(top_srcdir)/src/toxic_windows.h \
                $(top_srcdir)/src/windows.c \
                $(top_srcdir)/src/groupchat.c \
                $(top_srcdir)/src/groupchat.h \
                $(top_srcdir)/src/global_commands.c \
                $(top_srcdir)/src/global_commands.h \
                $(top_srcdir)/src/chat_commands.c \
                $(top_srcdir)/src/chat_commands.h \
                $(top_srcdir)/src/execute.c \
                $(top_srcdir)/src/execute.h \
                $(top_srcdir)/src/misc_tools.c \
                $(top_srcdir)/src/misc_tools.h \
                $(top_srcdir)/src/toxic_strings.c \
                $(top_srcdir)/src/toxic_strings.h \
                $(top_srcdir)/src/log.c \
                $(top_srcdir)/src/log.h \
                $(top_srcdir)/src/file_senders.c \
                $(top_srcdir)/src/file_senders.h

toxic_CFLAGS =                 -I$(top_srcdir) \
				$(NCURSES_CFLAGS) \
				$(LIBSODIUM_CFLAGS) \
				$(LIBTOXCORE_CFLAGS) \
				$(PTHREAD_CFLAGS)

toxic_CPPFLAGS = '-DTOXICVER="$(TOXIC_VERSION)"'

toxic_LDADD =	$(LIBTOXCORE_LDFLAGS) \
				$(LIBSODIUM_LDFLAGS) \
				$(NCURSES_LIBS) \
				$(LIBTOXCORE_LIBS) \
				$(LIBSODIUM_LIBS) \
                $(WINSOCK2_LIBS) \
                $(PTHREAD_LIBS)


# For audio support
if BUILD_AV

toxic_SOURCES += $(top_srcdir)/src/audio_call.c \
                 $(top_srcdir)/src/audio_call.h

toxic_CFLAGS += $(LIBTOXAV_CFLAGS) \
		$(OPENAL_CFLAGS)
		
toxic_LDADD += $(LIBTOXAV_LIBS) \
	       $(OPENAL_LIBS) 
endif
