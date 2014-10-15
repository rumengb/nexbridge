#ifndef __MDNS_AVAHI_H__
#define __MDNS_AVAHI_H__
#include "config.h"

int mdns_init(char *name, char *type, char *text, int port);
int mdns_start();
int mdns_stop();

#ifdef HAVE_LIBAVAHI_CLIENT
#ifdef HAVE_LIBAVAHI_COMMON
	#define HAVE_LIBAVAHI 1
#endif
#endif

#ifdef HAVE_LIBAVAHI
        #define HAVE_MDNS 1
#endif

#endif /*__MDNS_AVAHI_H__*/
