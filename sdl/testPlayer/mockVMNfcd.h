#ifndef __MOCKVM_NFCD_H_
#define __MOCKVM_NFCD_H_

#define MAX_CLIENTS 1000
#include "sensors.h"
#include "nfc.pb-c.h"
#include "logger.h"
#include "amqp_listen.h"
#include "amqp_send.h"

typedef struct s_nfc_params
{
    NfcPayload* payload;
    uint32_t toread;
    uint32_t nbevent;
    unsigned char* msg;
    unsigned int len;
} nfc_params;

void* mock_vmnfc_recv_poll(void* args);
void* new_playerNFC(void* params);

#endif
