
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include "sensors.h"
#include "player_nfc.h"
#include "nfc.pb-c.h"
#include "buffer_sizes.h"
#include "socket.h"

#include "amqp_listen.h"
#include "config_env.h"

#include "logger.h"
#include <pthread.h>

//_____________________

#define NCI_BRCM_CO_ID 0x2E

/* Define the message header size for all NCI Commands and Notifications.
*/
#define NCI_MSG_HDR_SIZE 3  /* per NCI spec */
#define NCI_DATA_HDR_SIZE 3 /* per NCI spec */
#define NCI_MAX_PAYLOAD_SIZE 0xFE
#define NCI_MAX_CTRL_SIZE 0xFF /* max control message size */
#define NCI_CTRL_INIT_SIZE 32  /* initial NFCC control payload size */
#define NCI_MAX_VSC_SIZE 0xFF
#define NCI_VSC_MSG_HDR_SIZE                                                                       \
    12 /* NCI header (3) + callback function pointer(8; use 8 to be safe) + HCIT (1 byte) */
#define NCI_TL_SIZE 2

#define NCI_ISO_DEP_MAX_INFO                                                                       \
    253 /* Max frame size (256) - Prologue (1) - Epilogue (2) in ISO-DEP, CID and NAD are not      \
           used*/
#define NCI_NFC_DEP_MAX_DATA                                                                       \
    251 /* Max payload (254) - Protocol Header (3) in NFC-DEP, DID and NAD are not used */

/* NCI Command and Notification Format:
 * 3 byte message header:
 * byte 0: MT PBF GID
 * byte 1: OID
 * byte 2: Message Length */
/* MT: Message Type (byte 0) */
#define NCI_MT_MASK 0xE0
#define NCI_MT_SHIFT 5
#define NCI_MT_DATA 0x00
#define NCI_MT_CMD 1 /* (NCI_MT_CMD << NCI_MT_SHIFT) = 0x20 */
#define NCI_MT_RSP 2 /* (NCI_MT_RSP << NCI_MT_SHIFT) = 0x40 */
#define NCI_MT_NTF 3 /* (NCI_MT_NTF << NCI_MT_SHIFT) = 0x60 */
#define NCI_MT_CFG 4 /* (NCI_MT_CFG << NCI_MT_SHIFT) = 0x80 */

#define NCI_MTS_CMD 0x20
#define NCI_MTS_RSP 0x40
#define NCI_MTS_NTF 0x60
#define NCI_MTS_CFG 0x80

#define NCI_NTF_BIT 0x80 /* the tNFC_VS_EVT is a notification */
#define NCI_RSP_BIT 0x40 /* the tNFC_VS_EVT is a response     */

/* for internal use only; not from specification */
/* the following 2 flags are used in layer_specific for fragmentation/reassembly of data packets */
#define NCI_LS_DATA 0x00
#define NCI_LS_DATA_PBF 0x01

/* PBF: Packet Boundary Flag (byte 0) */
#define NCI_PBF_MASK 0x10
#define NCI_PBF_SHIFT 4
#define NCI_PBF_NO_OR_LAST 0x00 /* not fragmented or last fragment */
#define NCI_PBF_ST_CONT 0x10    /* start or continuing fragment */

/* GID: Group Identifier (byte 0) */
#define NCI_GID_MASK 0x0F
#define NCI_GID_SHIFT 0
#define NCI_GID_CORE 0x00      /* 0000b NCI Core group */
#define NCI_GID_RF_MANAGE 0x01 /* 0001b RF Management group */
#define NCI_GID_EE_MANAGE 0x02 /* 0010b NFCEE Management group */
#define NCI_GID_PROP 0x0F      /* 1111b Proprietary */
/* 0111b - 1110b RFU */

/* OID: Opcode Identifier (byte 1) */
#define NCI_OID_MASK 0x3F
#define NCI_OID_SHIFT 0

/* For routing */
#define NCI_DH_ID 0 /* for DH */
/* To identify the loopback test */
#define NCI_TEST_ID 0xFE /* for loopback test */

/* Destination Type */
#define NCI_DEST_TYPE_NFCC 1   /* NFCC - loopback */
#define NCI_DEST_TYPE_REMOTE 2 /* Remote NFC Endpoint */
#define NCI_DEST_TYPE_NFCEE 3  /* NFCEE */

/* builds byte0 of NCI Command and Notification packet */
#define NCI_MSG_BLD_HDR0(p, mt, gid) *(p)++ = (uint8_t)(((mt) << NCI_MT_SHIFT) | (gid));

#define NCI_MSG_PBLD_HDR0(p, mt, pbf, gid)                                                         \
    *(p)++ = (uint8_t)(((mt) << NCI_MT_SHIFT) | ((pbf) << NCI_PBF_SHIFT) | (gid));

/* builds byte1 of NCI Command and Notification packet */
#define NCI_MSG_BLD_HDR1(p, oid) *(p)++ = (uint8_t)(((oid) << NCI_OID_SHIFT));

/* parse byte0 of NCI packet */
#define NCI_MSG_PRS_HDR0(p, mt, pbf, gid)                                                          \
    mt = (*(p) &NCI_MT_MASK) >> NCI_MT_SHIFT;                                                      \
    pbf = (*(p) &NCI_PBF_MASK) >> NCI_PBF_SHIFT;                                                   \
    gid = *(p)++ & NCI_GID_MASK;

/* parse MT and PBF bits of NCI packet */
#define NCI_MSG_PRS_MT_PBF(p, mt, pbf)                                                             \
    mt = (*(p) &NCI_MT_MASK) >> NCI_MT_SHIFT;                                                      \
    pbf = (*(p) &NCI_PBF_MASK) >> NCI_PBF_SHIFT;

/* parse byte1 of NCI Cmd/Ntf */
#define NCI_MSG_PRS_HDR1(p, oid)                                                                   \
    oid = (*(p) &NCI_OID_MASK);                                                                    \
    (p)++;

/* NCI Data Format:
 * byte 0: MT(0) PBF CID
 * byte 1: RFU
 * byte 2: Data Length */
/* CID: Connection Identifier (byte 0) 1-0xF Dynamically assigned (by NFCC), 0 is predefined  */
#define NCI_CID_MASK 0x0F

/* builds 3-byte message header of NCI Data packet */
#define NCI_DATA_BLD_HDR(p, cid, len)                                                              \
    *(p)++ = (uint8_t)(cid);                                                                       \
    *(p)++ = 0;                                                                                    \
    *(p)++ = (uint8_t)(len);

#define NCI_DATA_PBLD_HDR(p, pbf, cid, len)                                                        \
    *(p)++ = (uint8_t)(((pbf) << NCI_PBF_SHIFT) | (cid));                                          \
    *(p)++ = 0;                                                                                    \
    *(p)++ = (len);

#define NCI_DATA_PRS_HDR(p, pbf, cid, len)                                                         \
    (pbf) = (*(p) &NCI_PBF_MASK) >> NCI_PBF_SHIFT;                                                 \
    (cid) = (*(p) &NCI_CID_MASK);                                                                  \
    p++;                                                                                           \
    p++;                                                                                           \
    (len) = *(p)++;

/* Logical target ID 0x01-0xFE */

#define BE_STREAM_TO_uint32_t(u32, p)                                                              \
    {                                                                                              \
        u32 = ((uint32_t)(*((p) + 3)) + ((uint32_t)(*((p) + 2)) << 8) +                            \
               ((uint32_t)(*((p) + 1)) << 16) + ((uint32_t)(*(p)) << 24));                         \
        (p) += 4;                                                                                  \
    }

typedef struct
{
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
} BT_HDR;

/* Define the status code returned from the Validate, Parse or Build functions
*/
enum
{
    NDEF_OK, /* 0 - OK                                   */

    NDEF_REC_NOT_FOUND,         /* 1 - No record matching the find criteria */
    NDEF_MSG_TOO_SHORT,         /* 2 - Message was too short (< 3 bytes)    */
    NDEF_MSG_NO_MSG_BEGIN,      /* 3 - No 'begin' flag at start of message  */
    NDEF_MSG_NO_MSG_END,        /* 4 - No 'end' flag at end of message      */
    NDEF_MSG_EXTRA_MSG_BEGIN,   /* 5 - 'begin' flag after start of message  */
    NDEF_MSG_UNEXPECTED_CHUNK,  /* 6 - Unexpected chunk found               */
    NDEF_MSG_INVALID_EMPTY_REC, /* 7 - Empty record with non-zero contents  */
    NDEF_MSG_INVALID_CHUNK,     /* 8 - Invalid chunk found                  */
    NDEF_MSG_LENGTH_MISMATCH,   /* 9 - Overall message length doesn't match */
    NDEF_MSG_INSUFFICIENT_MEM   /* 10 - Insuffiecient memory to add record  */
};
typedef uint8_t tNDEF_STATUS;

/* Definitions for tNFA_TNF (NDEF type name format ID) */
#define NFA_TNF_EMPTY NDEF_TNF_EMPTY          /* Empty or no type specified                       */
#define NFA_TNF_WKT NDEF_TNF_WKT              /* NFC Forum well-known type [NFC RTD]              */
#define NFA_TNF_RFC2046_MEDIA NDEF_TNF_MEDIA  /* Media-type as defined in RFC 2046 [RFC 2046] */
#define NFA_TNF_RFC3986_URI NDEF_TNF_URI      /* Absolute URI as defined in RFC 3986 [RFC 3986]   */
#define NFA_TNF_EXTERNAL NDEF_TNF_EXT         /* NFC Forum external type [NFC RTD]                */
#define NFA_TNF_UNKNOWN case NDEF_TNF_UNKNOWN /* Unknown */
#define NFA_TNF_UNCHANGED NDEF_TNF_UNCHANGED  /* Unchanged */
#define NFA_TNF_RESERVED NDEF_TNF_RESERVED    /* Reserved                                         */
#define NFA_TNF_DEFAULT case 0xFF             /* Used to register default NDEF type handler       */
typedef uint8_t tNFA_TNF;

#define NDEF_MB_MASK 0x80  /* Message Begin */
#define NDEF_ME_MASK 0x40  /* Message End */
#define NDEF_CF_MASK 0x20  /* Chunk Flag */
#define NDEF_SR_MASK 0x10  /* Short Record */
#define NDEF_IL_MASK 0x08  /* ID Length */
#define NDEF_TNF_MASK 0x07 /* Type Name Format */

/* NDEF Type Name Format */
#define NDEF_TNF_EMPTY 0     /* Empty (type/id/payload len =0) */
#define NDEF_TNF_WKT 1       /* NFC Forum well-known type/RTD */
#define NDEF_TNF_MEDIA 2     /* Media-type as defined in RFC 2046 */
#define NDEF_TNF_URI 3       /* Absolute URI as defined in RFC 3986 */
#define NDEF_TNF_EXT 4       /* NFC Forum external type/RTD */
#define NDEF_TNF_UNKNOWN 5   /* Unknown (type len =0) */
#define NDEF_TNF_UNCHANGED 6 /* Unchanged (type len =0) */
#define NDEF_TNF_RESERVED 7  /* Reserved */

void createBufNdef_TypeURI(uint8_t* strIN, int sizLen, uint8_t* strOUT);
void createBufNdef_TypeText(uint8_t* strIN, int sizLen, uint8_t* strOUT);
void createBufNdef_TypeSmartPoster(uint8_t* strIN, uint8_t* strIN2, int sizLen, uint8_t* strOUT);

void vshort_actidata(uint8_t* strIN, int sizLen, uint8_t* strOUT);
void vshort_sendata(uint8_t* strIN, int sizLen, uint8_t* strOUT);

int codeNFC(NfcPayload* nfcData, uint8_t* msg);

/*******************************************************************************
**
** Function         NDEF_RecGetType
**
** Description      This function gets a pointer to the record type for the given NDEF record.
**
** Returns          Pointer to Type (NULL if none). TNF and len are filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetType(uint8_t* p_rec, uint8_t* p_tnf, uint8_t* p_type_len);

/*******************************************************************************
**
** Function         NDEF_RecGetId
**
** Description      This function gets a pointer to the record id for the given NDEF record.
**
** Returns          Pointer to Id (NULL if none). ID Len is filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetId(uint8_t* p_rec, uint8_t* p_id_len);

/*******************************************************************************
**
** Function         NDEF_RecGetPayload
**
** Description      This function gets a pointer to the payload for the given NDEF record.
**
** Returns          a pointer to the payload (or NULL none). Payload len filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetPayload(uint8_t* p_rec, uint32_t* p_payload_len);

/*******************************************************************************
**
** Function         NDEF_MsgValidate
**
** Description      This function validates an NDEF message.
**
** Returns          TRUE if all OK, or FALSE if the message is invalid.
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgValidate(uint8_t* p_msg, uint32_t msg_len, int b_allow_chunks);
