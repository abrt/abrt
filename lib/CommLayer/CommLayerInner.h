#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include "Observer.h"

void comm_layer_inner_init(CObserver *pObs);

/* Ask a client to warn the user about a non-fatal, but unexpected condition.
 * In GUI, it will usually be presented as a popup message.
 */
void comm_layer_inner_warning(const std::string& pMessage);
/* Logs a message to a client.
 * In UI, it will usually appear as a new status line message in GUI,
 * or as a new message line in CLI.
 */
void comm_layer_inner_status(const std::string& pMessage);

#endif /* COMMLAYERINNER_H_ */
