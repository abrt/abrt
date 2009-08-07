#ifndef COMMLAYERINNER_H_
#define COMMLAYERINNER_H_

#include "Observer.h"

void comm_layer_inner_init(CObserver *pObs);
void comm_layer_inner_debug(const std::string& pMessage);
void comm_layer_inner_warning(const std::string& pMessage);
void comm_layer_inner_status(const std::string& pMessage);

#endif /* COMMLAYERINNER_H_ */
