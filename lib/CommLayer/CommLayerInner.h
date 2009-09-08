#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include "Observer.h"

void init_daemon_logging(CObserver *pObs);

/*
 * Set client's name (dbus ID). NULL unsets it.
 */
void set_client_name(const char* name);
/* Ask a client to warn the user about a non-fatal, but unexpected condition.
 * In GUI, it will usually be presented as a popup message.
 */
void warn_client(const std::string& pMessage);
/* Logs a message to a client.
 * In UI, it will usually appear as a new status line message in GUI,
 * or as a new message line in CLI.
 */
void update_client(const std::string& pMessage);

#endif /* COMMLAYERINNER_H_ */

