#ifndef UNDOCUMENTED_APIS_H
#define UNDOCUMENTED_APIS_H

#include <PalmOS.h>
#include <stdint.h>

uint16_t (*customCall_HwrADC)(uint16_t mode, uint16_t* returnData);

Boolean initUndocumentedApiHandlers(void);

#endif
