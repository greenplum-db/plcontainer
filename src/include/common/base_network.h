#ifndef _BASE_NETWORK_H
#define _BASE_NETWORK_H

int plcListenServer(const char *network, const char *address);
int plcDialToServer(const char *network, const char *address);

#endif
