#ifndef __MDNS_AVAHI_H__
#define __MDNS_AVAHI_H__

int mdns_init(char *name, char *type, char *text, int port);
int mdns_start();
int mdns_stop();

#endif /*__MDNS_AVAHI_H__*/
