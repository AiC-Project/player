#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "mockVMNfcd.h"
#include "logger.h"
#include <pthread.h>

#define LOG_TAG "mockVMNfcd"

#include "mockVMLibNci.h"

#include <sys/types.h>

#define NFC_UPDATE_PERIOD 1 /* period in sec between 2 nfc fix emission */

#define SIM_NFC_PORT 22800

static int start_server(uint16_t port)
{
    int server = -1;
    struct sockaddr_in srv_addr;

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(port);

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOGI(" NFC Unable to create socket\n");
        return -1;
    }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(server, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0)
    {
        LOGI(" NFC Unable to bind socket, errno=%d\n", errno);
        return -1;
    }

    return server;
}

static int wait_for_client(int server)
{
    int client = -1;

    if (listen(server, 1) < 0)
    {
        LOGI("Unable to listen to socket, errno=%d\n", errno);
        return -1;
    }

    client = accept(server, NULL, 0);

    if (client < 0)
    {
        LOGI("Unable to accept socket for main conection, errno=%d\n", errno);
        return -1;
    }

    return client;
}

NfcPayload* readNfcBody(int csock, uint32_t siz)
{
    int bytecount = 0;

    NfcPayload* payloadnfc;
    //    nfc_payload__init(payloadnfc);

    void* buffer = malloc(siz);

    LOGI(" readBody --  inSize %d", siz);

    // Read the entire buffer including the hdr
    if ((bytecount = recv(csock, buffer, siz, MSG_WAITALL)) == -1)
    {
        LOGI("Error receiving data %d", bytecount);
    }

    LOGI(" readBody --  Second read byte count is %d", bytecount);

    payloadnfc = nfc_payload__unpack(NULL, bytecount, buffer);

    LOGI(" readBody payloadnfc->has_type %d", payloadnfc->has_type);
    LOGI(" readBody payloadnfc->has_lang %d", payloadnfc->has_lang);

    LOGI(" readBody payloadnfc->type %d", payloadnfc->type);
    LOGI(" readBody payloadnfc->lang %d", payloadnfc->lang);
    LOGI(" readBody payloadnfc->text %s", payloadnfc->text);
    LOGI(" readBody payloadnfc->tittle %s", payloadnfc->tittle);

    return payloadnfc;
}

void* mock_vmnfc_recv_poll(void* args)
{
    int sim_server = -1;
    int sim_client = -1;

    char inbuf[256];
    int bytecount = 0;

    int zSiz = 0;
    uint8_t msg[1024];
    uint8_t msg2[1024];
    int msg_len = 0;

    nfc_params* params = (nfc_params*) args;

    if ((sim_server = start_server(22800)) == -1)
    {
        LOGI(" NFC Unable to create socket\n");
        return NULL;
    }

    // Listen for main connection
    while ((sim_client = wait_for_client(sim_server)) != -1)
    {
        // Update NFC info every NFC_UPDATE_PERIOD seconds
        sleep(NFC_UPDATE_PERIOD);

        memset(inbuf, '\0', 256);

        // Peek into the socket and get the packet size
        // bytecount = send (sim_client,cmdbuf, 3, 0 ) ;
        recv(sim_client, inbuf, 3, MSG_PEEK);

        LOGI("NFC - local nfc server recv inbuf %x%x%x", inbuf[0], inbuf[1], inbuf[2]);
        int tt = strcmp(inbuf, "303");
        if (tt == 0)
        {
            bytecount = recv(sim_client, inbuf, 5, MSG_WAITALL);
            LOGI("NFC - local nfc server recv inbuf - bytecount %d", bytecount);
            zSiz = (int) strtol(inbuf + 3, NULL, 10);
        }
        LOGI("NFC - local nfc server recv inbuf tt=%d - zSiz=%d", tt, zSiz);

        LOGI("NFC:: reveivinf data");
        params->payload = readNfcBody(sim_client, params->toread);
        params->nbevent++;

        NfcPayload* nfcData = params->payload;

        msg_len = codeNFC(nfcData, msg);

        vshort_sendata(msg, msg_len, msg2);

        for (int ii = 0; ii < msg_len; ii++)
            printf("0x%0x-", msg2[ii]);

        // size = tcp_write_buff( sock, msg2, len2send );

        memcpy(params->msg, msg2, 1024);
        params->len = msg_len;

        LOGI("Stop events read, we have read enough %d %d %s ", params->payload->lang,
             params->payload->type, params->payload->text);
        pthread_exit(NULL);

        LOGI("NFC:: First read byte count is %d", bytecount);
    }
    return NULL;
}
