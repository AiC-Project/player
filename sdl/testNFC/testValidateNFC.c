#include "buffer_sizes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define BOOLEAN int
#define TRUE 1
#define FALSE 0

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

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

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

void printError(tNDEF_STATUS status)
{
    switch (status)
    {
    case NDEF_OK:
        printf("    NDEF_OK==> 0 - OK \n");
        break;
    case NDEF_REC_NOT_FOUND:
        printf("    NDEF_REC_NOT_FOUND ==>  1 - No record matching the find criteria \n");
        break;
    case NDEF_MSG_TOO_SHORT:
        printf("    NDEF_MSG_TOO_SHORT ==>  2 - Message was too short (< 3 bytes)    \n");
        break;
    case NDEF_MSG_NO_MSG_BEGIN:
        printf("    NDEF_MSG_NO_MSG_BEGIN ==>  - No 'begin' flag at start of message  \n");
        break;
    case NDEF_MSG_NO_MSG_END:
        printf("    NDEF_MSG_NO_MSG_END ==>  - No 'end' flag at end of message      \n");
        break;
    case NDEF_MSG_EXTRA_MSG_BEGIN:
        printf("    NDEF_MSG_EXTRA_MSG_BEGIN ==>  5 - 'begin' flag after start of message  \n");
        break;
    case NDEF_MSG_UNEXPECTED_CHUNK:
        printf("    NDEF_MSG_UNEXPECTED_CHUNK ==>  6 - Unexpected chunk found               \n");
        break;
    case NDEF_MSG_INVALID_EMPTY_REC:
        printf("    NDEF_MSG_INVALID_EMPTY_REC ==>  7 - Empty record with non-zero contents  \n");
        break;
    case NDEF_MSG_INVALID_CHUNK:
        printf("    NDEF_MSG_INVALID_CHUNK ==>  8 - Invalid chunk found                  \n");
        break;
    case NDEF_MSG_LENGTH_MISMATCH:
        printf("    NDEF_MSG_LENGTH_MISMATCH ==>  9 - Overall message length doesn't match \n");
        break;
    case NDEF_MSG_INSUFFICIENT_MEM:
        printf("    NDEF_MSG_INSUFFICIENT_MEM ==>  10 - Insuffiecient memory to add record  \n");
        break;
    }
    return;
}

tNDEF_STATUS NDEF_MsgValidate(uint8_t* p_msg, uint32_t msg_len, BOOLEAN b_allow_chunks)
{
    uint8_t* p_rec = p_msg;
    uint8_t* p_end = p_msg + msg_len;
    uint8_t rec_hdr = 0, type_len, id_len;
    int count;
    uint32_t payload_len;
    BOOLEAN bInChunk = FALSE;

    if ((p_msg == NULL) || (msg_len < 3))
        return (NDEF_MSG_TOO_SHORT);

    /* The first record must have the MB bit set */
    if ((*p_msg & NDEF_MB_MASK) == 0)
        return (NDEF_MSG_NO_MSG_BEGIN);

    /* The first record cannot be a chunk */
    if ((*p_msg & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
        return (NDEF_MSG_UNEXPECTED_CHUNK);

    for (count = 0; p_rec < p_end; count++)
    {
        /* if less than short record header */
        if (p_rec + 3 > p_end)
            return (NDEF_MSG_TOO_SHORT);

        rec_hdr = *p_rec++;
        printf(" A-> %x", rec_hdr);

        /* The second and all subsequent records must NOT have the MB bit set */
        if ((count > 0) && (rec_hdr & NDEF_MB_MASK))
            return (NDEF_MSG_EXTRA_MSG_BEGIN);

        /* Type field length */
        type_len = *p_rec++;
        printf(" type_len-> %d", type_len);

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
        {
            payload_len = *p_rec++;
            printf(" payload_len-> %d", payload_len);
        }
        else
        {
            /* if less than 4 bytes payload length */
            if (p_rec + 4 > p_end)
                return (NDEF_MSG_TOO_SHORT);

            BE_STREAM_TO_uint32_t(payload_len, p_rec);
        }

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
        {
            /* if less than 1 byte ID field length */
            if (p_rec + 1 > p_end)
                return (NDEF_MSG_TOO_SHORT);

            id_len = *p_rec++;
            printf(" id_len-> %d", id_len);
        }
        else
            id_len = 0;

        /* A chunk must have type "unchanged", and no type or ID fields */
        if (rec_hdr & NDEF_CF_MASK)
        {
            printf(" E-> %x", rec_hdr);
            if (!b_allow_chunks)
                return (NDEF_MSG_UNEXPECTED_CHUNK);

            /* Inside a chunk, the type must be unchanged and no type or ID field i sallowed */
            if (bInChunk)
            {
                if ((type_len != 0) || (id_len != 0) ||
                    ((rec_hdr & NDEF_TNF_MASK) != NDEF_TNF_UNCHANGED))
                    return (NDEF_MSG_INVALID_CHUNK);
            }
            else
            {
                /* First record of a chunk must NOT have type "unchanged" */
                if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
                    return (NDEF_MSG_INVALID_CHUNK);

                bInChunk = TRUE;
            }
        }
        else
        {
            /* This may be the last guy in a chunk. */
            if (bInChunk)
            {
                if ((type_len != 0) || (id_len != 0) ||
                    ((rec_hdr & NDEF_TNF_MASK) != NDEF_TNF_UNCHANGED))
                    return (NDEF_MSG_INVALID_CHUNK);

                bInChunk = FALSE;
            }
            else
            {
                /* If not in a chunk, the record must NOT have type "unchanged" */
                if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
                    return (NDEF_MSG_INVALID_CHUNK);
            }
        }

        /* An empty record must NOT have a type, ID or payload */
        if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_EMPTY)
        {
            printf("An empty record must NOT have a type, ID or payload \n");
            if ((type_len != 0) || (id_len != 0) || (payload_len != 0))
                return (NDEF_MSG_INVALID_EMPTY_REC);
        }

        if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNKNOWN)
        {
            printf(" F-> %x", rec_hdr);
            if (type_len != 0)
                return (NDEF_MSG_LENGTH_MISMATCH);
        }

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);

        if (rec_hdr & NDEF_ME_MASK)
        {
            printf("p_rec=%p", p_rec);
            break;
        }
        else
        {
            printf("p_rec");
        }

        rec_hdr = 0;
    }

    /* The last record should have the ME bit set */
    if ((rec_hdr & NDEF_ME_MASK) == 0)
    {
        return (NDEF_MSG_NO_MSG_END);
    }

    /* p_rec should equal p_end if all the length fields were correct */
    if (p_rec != p_end)
    {
        printf(" p_rec-> %p", p_rec);
        printf(" p_end-> %p", p_end);
        return (NDEF_MSG_LENGTH_MISMATCH);
    }

    return (NDEF_OK);
}

void printbuff0(uint8_t* p_rec, uint8_t* repere)
{
    uint32_t count;
    for (count = 0; count < 35; count++)
        printf("%x", p_rec[count]);
    printf("   <- %s \n", repere);
}

void printbuff(uint8_t* p_rec, int len)
{
    int count;
    printf("\n{");
    for (count = 0; count < len - 1; count++)
        printf("0x%0x,", p_rec[count]);

    printf("0x%0x", p_rec[len - 1]);
    printf("}\n count %d\n", count + 1);
}

/*******************************************************************************
**
** Function         NDEF_RecGetType
**
** Description      This function gets a pointer to the record type for the given NDEF record.
**
** Returns          Pointer to Type (NULL if none). TNF and len are filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetType(uint8_t* p_rec, uint8_t* p_tnf, uint8_t* p_type_len)
{
    uint8_t rec_hdr, type_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Skip the payload length */
    if (rec_hdr & NDEF_SR_MASK)
        p_rec += 1;
    else
        p_rec += 4;

    /* Skip ID field Length, if present */
    if (rec_hdr & NDEF_IL_MASK)
        p_rec++;

    /* At this point, p_rec points to the start of the type field.  */
    *p_type_len = type_len;
    *p_tnf = rec_hdr & NDEF_TNF_MASK;

    if (type_len == 0)
        return (NULL);
    else
        return (p_rec);
}

/*******************************************************************************
**
** Function         NDEF_RecGetId
**
** Description      This function gets a pointer to the record id for the given NDEF record.
**
** Returns          Pointer to Id (NULL if none). ID Len is filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetId(uint8_t* p_rec, uint8_t* p_id_len)
{
    uint8_t rec_hdr, type_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Skip the payload length */
    if (rec_hdr & NDEF_SR_MASK)
        p_rec++;
    else
        p_rec += 4;

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
        *p_id_len = *p_rec++;
    else
        *p_id_len = 0;

    /* p_rec now points to the start of the type field. The ID field follows it */
    if (*p_id_len == 0)
        return (NULL);
    else
        return (p_rec + type_len);
}

/*******************************************************************************
**
** Function         NDEF_RecGetPayload
**
** Description      This function gets a pointer to the payload for the given NDEF record.
**
** Returns          a pointer to the payload (or NULL none). Payload len filled in.
**
*******************************************************************************/
uint8_t* NDEF_RecGetPayload(uint8_t* p_rec, uint32_t* p_payload_len)
{
    uint8_t rec_hdr, type_len, id_len;
    uint32_t payload_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Next is the payload length (1 or 4 bytes) */
    if (rec_hdr & NDEF_SR_MASK)
        payload_len = *p_rec++;
    else
        BE_STREAM_TO_uint32_t(payload_len, p_rec);

    *p_payload_len = payload_len;

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
        id_len = *p_rec++;
    else
        id_len = 0;

    /* p_rec now points to the start of the type field. The ID field follows it, then the payload */
    if (payload_len == 0)
        return (NULL);
    else
        return (p_rec + type_len + id_len);
}

void createBufNdef_TypeURI(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    //  0  ,  1 , 2  ,  3 |,|  4 ,  5 ,  6 ,  7 ,  8 ,  9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 ,
    //  18 ,  19};
    //{0xD1,0x01,0x10,0x55|,|
    // 0x03,0x62,0x6c,0x6f,0x67,0x2e,0x7a,0x65,0x6e,0x69,0x6b,0x61,0x2e,0x63,0x6f,0x6d};
    //     ,    ,    ,    |,|    ,  b ,  l ,  o ,  g ,  . ,  z ,  e ,  n ,  i ,  k ,  a ,  . ,  c ,
    //     o ,  m };
    // D1 : MB=1, ME=1, CR=0, SR=1, IL=0, TNF=001
    // 01 : type sur 1 octet
    // 10 : payload sur 16 octets
    // 55 : type U (URI)
    // 03 : préfixe http://
    // 626C6F672E7A656E696B612E636F6D : blog.zenika.com

    int offset = 1;  // for prefix type
    uint8_t rec_hdr = 0x00;

    printf("createBufNdef_TypeURI \n");
    // rec_hdr |=  NDEF_TNF_URI;
    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;
    // rec_hdr |=  NDEF_IL_MASK;
    // rec_hdr |=  ~NDEF_CF_MASK;

    strOUT[0] = rec_hdr;
    strOUT[1] = 0x01;
    strOUT[2] = sizLen + offset;
    strOUT[3] = 0x55;

    printf("createBufNdef_TypeURI hdr=%x08\n", *strOUT);

    strOUT[4] = 0x03;
    strncat((char*) strOUT, (char*) strIN, sizLen);
    // strOUT[sizLen+sizehdr]=NDEF_ME_MASK;

    printf(" Check tnf=%d & prefix%x", (short) strOUT[0] & 0x07, strOUT[4]);
    return;
}

void createBufNdef_TypeText(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int sizehdr = 4;

    int offset = 6;

    uint8_t p_new_hdr[sizehdr];
    uint8_t rec_hdr;
    bzero(p_new_hdr, sizeof(p_new_hdr));

    rec_hdr = 0x00;

    // rec_hdr |=  NDEF_TNF_URI;
    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;
    // rec_hdr |=  NDEF_IL_MASK;
    // rec_hdr |=  ~NDEF_CF_MASK;

    strOUT[0] = rec_hdr;
    strOUT[1] = 0x01;
    strOUT[2] = sizLen + offset;
    strOUT[3] = 0x54;  // T

    strOUT[4] = 0x05;  // x
    strOUT[5] = 0x65;  // e
    strOUT[6] = 0x6E;  // n
    strOUT[7] = 0x2D;  // n
    strOUT[8] = 0x55;  // U
    strOUT[9] = 0x53;  // S

    //          for (count = sizehdr + offset ; count<(sizLen+sizehdr); count ++ )
    //             strOUT[count] = strIN[count-sizehdr];
    //
    //             strOUT[sizLen+sizehdr] = strIN[sizLen];
    // strOUT[sizLen+sizehdr]=NDEF_ME_MASK;

    strncat((char*) strOUT, (char*) strIN, sizLen);

    printf(" tnf=%d", (short) p_new_hdr[0] & 0x07);
    return;
}

void createBufNdef_TypeSmartPoster(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int sizehdr = 4;
    int offset = 6;

    uint8_t p_new_hdr[sizehdr];
    bzero(p_new_hdr, sizeof(p_new_hdr));
    uint8_t rec_hdr;
    rec_hdr = 0x00;

    // rec_hdr |=  NDEF_TNF_URI;
    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;

    // rec_hdr |=  NDEF_IL_MASK;
    // rec_hdr |=  ~NDEF_CF_MASK;

    strOUT[0] = rec_hdr;
    strOUT[1] = 0x02;  // tnf 001
    strOUT[2] = sizLen + offset + 10;
    strOUT[3] = 0x53;  // S

    strOUT[4] = 0x70;  // p
    strOUT[5] = 0x91;  // x
    strOUT[6] = 0x01;  // x
    strOUT[7] = 0x10;  // x
    strOUT[8] = 0x55;  // U
    strOUT[9] = 0x03;  // x

    //          for (count = 0 ; count<sizehdr; count ++ )
    //             strOUT[count] = p_new_hdr[count];
    //
    //          for (count = sizehdr + offset ; count<(sizLen+sizehdr +offset); count ++ )
    //             strOUT[count] = strIN[count-sizehdr+offset];
    //
    //             strOUT[sizLen+sizehdr+offset] = strIN[sizLen];
    //          //strOUT[sizLen+sizehdr]=NDEF_ME_MASK;

    strncat((char*) strOUT, (char*) strIN, sizLen);

    strOUT[sizehdr + offset + sizLen + 0] = 0x51;
    strOUT[sizehdr + offset + sizLen + 1] = 0x01;
    strOUT[sizehdr + offset + sizLen + 2] = 0x07;
    strOUT[sizehdr + offset + sizLen + 3] = 0x54;
    strOUT[sizehdr + offset + sizLen + 4] = 0x02;
    strOUT[sizehdr + offset + sizLen + 5] = 0x66;
    strOUT[sizehdr + offset + sizLen + 6] = 0x72;
    strOUT[sizehdr + offset + sizLen + 7] = 0x42;
    strOUT[sizehdr + offset + sizLen + 8] = 0x6C;
    strOUT[sizehdr + offset + sizLen + 9] = 0x6F;
    strOUT[sizehdr + offset + sizLen + 10] = 0x67;

    printf(" tnf=%d", (short) p_new_hdr[0] & 0x07);
    return;
}

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;
#define INVALID_SOCKET -1

SOCKET open_socket(char* ip, short port)
{
    SOCKET sock;
    SOCKADDR_IN sin;
    int yes = 1;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        printf("Can’t create socket...\n");
        close(sock);
        return (INVALID_SOCKET);
    }

    memset(&sin, 0, sizeof(sin));
    inet_aton("192.168.122.129", &sin.sin_addr);
    // inet_aton("192.168.122.42", &sin.sin_addr);

    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;

    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));  // 2

    if (connect(sock, (SOCKADDR*) &sin, sizeof(SOCKADDR)) == INVALID_SOCKET)
    {
        printf("Can't connect socket %d to %s:%d, invalid socket...\n", sock, ip, port);
        close(sock);
        return (INVALID_SOCKET);
    }
    printf(" Socket %d connected to port %s:%d \n", sock, ip, port);
    return (sock);
}

int tcp_write_buff(int sock, unsigned char* data, int len)
{
    return (send(sock, data, len, 0));
}

SOCKET getconnect()
{
    SOCKET player_fd;
    int flag_connect = 0;
    int _PORT = 22800;

    flag_connect = 0;

    while (!flag_connect)
    {
        player_fd = open_socket("192.168.122.129", _PORT);
        if (player_fd == INVALID_SOCKET)
        {
            printf("Cannot connect to aicNfcd (TCP %d) - VM down or access denied\n", _PORT);
            close(player_fd);
            sleep(1);
        }
        else
        {
            printf("Connected to aicNfcd (TCP %d)\n", _PORT);
            flag_connect = 1;
        }
    }

    return player_fd;
}
/*
int w_msg2sock( SOKCKET player_fd,  char *write_buffer )
{

     char inbuf[3];

     //     while(strncmp(inbuf,"202",3)!=0)
//     {
//         int bytecount = recv (player_fd, inbuf, 3, MSG_WAITALL ) ;
//         printf( "Connected to aicNfcd (TCP %d) Receiving \n",   bytecount);
//         sleep(1);
//     }

    int size = tcp_write_buff( player_fd, write_buffer, strlen(write_buffer) );
    printf( "Write to aicNfcd len %d\n",   size);


    return 1;
}*/

void vshort_actidata(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int ii;
    int offset = 3;

    //      printf("\n--------------------------\n");
    //     for( ii=0;ii<sizLen; ii++)
    //         printf("0x%0x-",strIN[ii]);
    //
    //      printf("\n--------------------------\n");

    for (ii = 0; ii <= sizLen + offset; ii++)
        strOUT[ii + offset] = strIN[ii];

    strOUT[0] = 0x00;
    strOUT[1] = 0x00;
    strOUT[2] = 0x00;

//     for( ii=0;ii<sizLen; ii++)
//         printf("0x%0x-",strOUT[ii]);
//
//      printf("\n--------------------------\n");
//
//     for( ii=0;ii<sizLen; ii++)
//         printf("0x%0x-",strOUT[ii]);

#define NCI_MSG_RF_INTF_ACTIVATED 5

    NCI_MSG_BLD_HDR0(strOUT, NCI_MT_NTF, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1(strOUT, NCI_MSG_RF_INTF_ACTIVATED);

    printf("\n--------------------------\n");
    for (ii = 0; ii < sizLen; ii++)
        printf("0x%0x-", strOUT[ii]);

    printf("\n--------------------------\n");
}

void vshort_sendata(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int ii;
    int offset = 3;

    //      printf("\n--------------------------\n");
    //     for( ii=0;ii<sizLen; ii++)
    //         printf("0x%0x-",strIN[ii]);
    //
    //      printf("\n--------------------------\n");

    for (ii = 0; ii <= sizLen + offset; ii++)
        strOUT[ii + offset] = strIN[ii];

    strOUT[0] = 0x00;
    strOUT[1] = 0x00;
    strOUT[2] = 0x00;

    //     for( ii=0;ii<sizLen; ii++)
    //         printf("0x%0x-",strOUT[ii]);
    //
    //      printf("\n--------------------------\n");

    NCI_DATA_BLD_HDR(strOUT, 0, sizLen);

    //     for( ii=0;ii<sizLen; ii++)
    //         printf("0x%0x-",strOUT[ii]);
}

#if 0
static int wait_for_client(int server)
{
    int client = -1;

    if (listen(server, 1) < 0)
    {
        printf("Unable to listen to socket, errno=%d\n", errno);
        return -1;
    }

    printf("wait_for_client\n");
    client = accept(server, NULL, 0);

    if (client < 0)
    {
        printf("Unable to accept socket for main conection, errno=%d\n", errno);
        return -1;
    }

    return client;
}
#endif

int main(int argc, char* argv[])
{
    (void) argc;
    // uint8_t p_new_rec[] = {0xFF,      0x00,   0x1E,   0x00, 0x6, 0x2, 0x6, 0xC, 0x6, 0xF, 0x6,
    // 0x7, 0x2, 0xE, 0x7, 0xA, 0x6, 0x5, 0x6, 0xE, 0x6, 0x9, 0x6, 0xB, 0x6, 0x1, 0x2, 0xE, 0x6,
    // 0x3, 0x6, 0xF, 0x6, 0xD};
    // uint8_t p_new_rec[] = {0xFF,      0x00,   0x1E,   0x00, 0x6, 0x2, 0x6, 0xC, 0x6, 0xF, 0x6,
    // 0x7, 0x2, 0xE, 0x7, 0xA, 0x6, 0x5, 0x6, 0xE, 0x6, 0x9, 0x6, 0xB, 0x6, 0x1, 0x2, 0xE, 0x6,
    // 0x3, 0x6, 0xF, 0x6, 0xD};
    //                            ^          ^       ^       ^
    //                            |          |       |       |
    //          rec_hdr |=  NDEF_TNF_URI;    |       |       |
    //          rec_hdr |=  NDEF_SR_MASK;    |       |       |
    //          rec_hdr |= ~NDEF_IL_MASK;    |       |       |
    //          rec_hdr |= ~NDEF_CF_MASK;    |       |       |
    //                                       |       |       |
    //                                       |       |       |
    //                                      tnf      |       |
    //                                            payload    |
    //                                                       |
    //                                                      id_len
    // msg_len =   4 + tnf +  payload  + id_len
    // here -> 30 + 4
    //

    //  0  ,  1 , 2  ,  3 |,|  4 ,  5 ,  6 ,  7 ,  8 ,  9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 ,
    //  18 ,  19};
    //{0xD1,0x01,0x10,0x54|,|0x05,0x65,0x6e,0x2d,0x55,0x53,0x48,0x65,0x6C,0x6C,0x6F,0x20,0x57,0x6F,0x72,0x6C};
    //     ,    ,    ,    |,|    ,  e ,  n ,  - ,  U ,  S ,  H ,  e ,  l ,  l ,  o ,    ,  W ,  o ,
    //     r ,  l };
    // D1 : MB=1, ME=1, CR=0, SR=1, IL=0, TNF=001
    // 01 : type sur 1 octet
    // 10 : payload sur 16 octets
    // 54 : type T (Text)
    // 05 : pas de préfixe
    // 656e2d555348656C6C6F20576F726C : Hello Worl
    //     ,    ,    ,    |,|    ,  F ,  r ,  H ,  e ,  l ,  l ,  o ,    ,  W ,  o ,  r ,  l ,  d ,
    //     ,  ! };

    printf(" args[0]:Type(0:Uri 1:Text 2:SmartPoster) ; [1]:Lang(0:En-US 1:Fr) ; "
           "[2]:payload(blahblah)\n");

    unsigned char* argbuff = (unsigned char*) malloc(sizeof(unsigned char) * 1024);
    int Type = atoi(argv[1]);
    int Lang = atoi(argv[2]);
    int sendORacti = atoi(argv[3]);

    int argLen = strlen(argv[4]);
    int offsetPrefix = 1;
    int sizehdr = 4;  // header + Prefix
    int msg_len = 0;

    uint8_t* msg = 0;
    uint8_t* msg2;
    uint8_t msg1[6] = "303";

    strncpy((char*) argbuff, argv[4], 1024);
    //   ccpy (argbuff, "blog.zenika.com"); argLen=20;
    printbuff(argbuff, argLen);
    printf("argLen=%d argBuff=%s\n", argLen, argbuff);

    int flag = 1;

    switch (Type)
    {
    case 0:
        printf("case 0 ; Type %d Lang %d\n", Type, Lang);
        sizehdr += offsetPrefix;
        msg_len = (argLen + sizehdr);
        msg = (uint8_t*) calloc(msg_len, sizeof(uint8_t));
        createBufNdef_TypeURI(argbuff, argLen, msg);
        break;

    case 1:  // Type Text need calculate HeaderSize
        offsetPrefix = 6;
        printf("case 1 ; Type %d Lang %d\n", Type, Lang);
        if (Lang == 0)
        {
            // sizehdr += 6 ;
            sizehdr += offsetPrefix;
            printf("Type Text Lang:En-US Sizehdr=%d \n", sizehdr);
            msg_len = (argLen + sizehdr);
            msg = (uint8_t*) calloc(msg_len, sizeof(uint8_t));
            createBufNdef_TypeText(argbuff, argLen, msg);
        }
        else if (Lang == 1)
        {
            // sizehdr += 3 ;
            sizehdr += offsetPrefix;
            printf("Type Text Lang:FR Sizehdr=%d \n", sizehdr);
            msg_len = (argLen + sizehdr);
            msg = (uint8_t*) calloc(msg_len, sizeof(uint8_t));
            createBufNdef_TypeText(argbuff, argLen, msg);
        }
        else
        {
            free(argbuff);
            return 0;
        }
        break;

    case 2:
        offsetPrefix = 6;
        printf("case 2 ; Type %d Lang %d\n", Type, Lang);
        sizehdr += offsetPrefix;
        msg_len = (argLen + sizehdr) + 11;
        msg = (uint8_t*) calloc(msg_len, sizeof(uint8_t));
        createBufNdef_TypeSmartPoster(argbuff, argLen, msg);
        break;
    case 3:
        printf("case 3 ; Type %d Lang %d\n", Type, Lang);
        sizehdr = 2;
        msg_len = (argLen + sizehdr);
        msg = (uint8_t*) calloc(msg_len, sizeof(uint8_t));
        vshort_sendata(argbuff, argLen, msg);
    // w_msg2sock(msg);
    // flag=1;
    default:
        printf("default ; Type %d Lang %d\n", Type, Lang);
        free(argbuff);
        return 0;
        break;
    }

    //     if (flag)
    //         return ;
    free(argbuff);

    printbuff(msg, msg_len);

    tNDEF_STATUS status = NDEF_MsgValidate(msg, msg_len, TRUE);

    printf("\nResult : \n Status Validate %d \n", status);
    printError(status);
    printf("\nDone !\n");

    int size, player_fd = -1;
    uint8_t p_tnf;
    uint8_t p_type_len;
    uint8_t p_id_len;
    uint32_t p_payload_len;

    NDEF_RecGetType(msg, &p_tnf, &p_type_len);

    NDEF_RecGetId(msg, &p_id_len);

    NDEF_RecGetPayload(msg, &p_payload_len);

    printf("p_tnf=%d ; p_type_len=%d ; p_id_len =%d; p_payload_len=%d\n", p_tnf, p_type_len,
           p_id_len, p_payload_len);

    uint8_t c[3];
    int len2send = strlen((char*) msg) + 3;
    snprintf((char*) c, sizeof(c), "%d", len2send);

    strncat((char*) msg1, (char*) c, sizeof(msg1) - strlen((char*) msg1) - 1);

    printbuff(msg1, 6);
    /*
        int test =(int)strtol( msg1+3, NULL, 10 );
        printf ( "\n--%d--\n" , test);
    */

    /*
        int rd_client, rd_server;


        if ((rd_server = start_server()) == -1) {
            ALOGE("unfcd_read_thread : start_server unable to create socket\n");
            return 0;
        }

       // Listen for main connection
        rd_client = wait_for_client(rd_server);
        //player_fd, inbuf, 3, MSG_WAITALL*/

    player_fd = getconnect();

    if (sendORacti == 0)
    {
        int16_t cmd;
        recv(player_fd, &cmd, sizeof(cmd), MSG_WAITALL);
        printf("\n received %d  \n", cmd);
        if (cmd != 1001)
        {
            printf("Unknown cmd : %d, exit\n", cmd);
            exit(-1);
        }
        else
        {
            flag = 0;
        }
    }

    if (flag)
    {
        int16_t cmd;
        recv(player_fd, &cmd, sizeof(cmd), MSG_DONTWAIT);
        printf("\n received %d  \n", cmd);
        if (cmd == 1002)
        {
            printf("NFC is off cmd : %d, exit\n", cmd);
            exit(-1);
        }
    }

    // w_msg2sock(msg1);
    size = tcp_write_buff(player_fd, msg1, 5);

    // w_msg2sock(msg);
    sleep(3);
    printf("\n writting msg1 %d %lu!\n", size, strlen((char*) msg1));

    msg2 = (uint8_t*) calloc(strlen((char*) msg), sizeof(uint8_t));

    if (sendORacti == 0)
    {
        vshort_sendata(msg, msg_len, msg2);
        // w_msg2sock(msg2);
        size = tcp_write_buff(player_fd, msg2, len2send);
        printf("\n writting sendata %d %d %lu!\n", size, len2send, strlen((char*) msg));
        printbuff(msg2, msg_len + 3);
    }
    else if (sendORacti == 2)
    {
        vshort_sendata(msg, msg_len, msg2);
        // w_msg2sock(msg2);
        size = tcp_write_buff(player_fd, msg2, len2send);
        printf("\n writting sendata %d %d %lu!\n", size, len2send, strlen((char*) msg));
        printbuff(msg2, msg_len + 3);
    }
    else
    {
        vshort_actidata(msg, msg_len, msg2);
        // w_msg2sock(msg2);
        size = tcp_write_buff(player_fd, msg2, len2send);
        printf("\n writting msg2 activate %d %d %lu!\n", size, len2send, strlen((char*) msg2));
        printbuff(msg2, msg_len + 3);
    }

    //      w_msg2sock(msg1);sleep(3);printf ("writting again msg1\n");
    //      w_msg2sock(msg1);sleep(3);printf ("writting again msg1\n");
    //      w_msg2sock(msg1);sleep(3);printf ("writting again msg1\n");
    //      w_msg2sock(msg1);sleep(3);printf ("writting again msg1\n");

    return 0;
}
