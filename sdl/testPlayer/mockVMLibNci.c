
#define LOG_TAG "mocklibnci"

#include "mockVMLibNci.h"
#include <string.h>

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

    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;

    strOUT[0] = rec_hdr;
    strOUT[1] = 0x01;
    strOUT[2] = sizLen + offset;
    strOUT[3] = 0x55;

    strOUT[4] = 0x03;
    strncat((char*) strOUT, (const char*) strIN, sizLen);

    return;
}

void createBufNdef_TypeText(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int offset = 6;
    uint8_t rec_hdr;

    rec_hdr = 0x00;

    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;

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

    strncat((char*) strOUT, (const char*) strIN, sizLen);

    return;
}

void createBufNdef_TypeSmartPoster(uint8_t* strIN, uint8_t* strIN2, int sizLen, uint8_t* strOUT)
{
    int sizehdr = 4;
    int offset = 6;

    uint8_t rec_hdr;
    rec_hdr = 0x00;

    rec_hdr |= NDEF_TNF_WKT;
    rec_hdr |= NDEF_ME_MASK;
    rec_hdr |= NDEF_MB_MASK;
    rec_hdr |= NDEF_SR_MASK;

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

    strncat((char*) strOUT, (const char*) strIN, sizLen);

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

    return;
}

void vshort_actidata(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int ii;
    int offset = 3;

    for (ii = 0; ii <= sizLen + offset; ii++)
        strOUT[ii + offset] = strIN[ii];

    strOUT[0] = 0x00;
    strOUT[1] = 0x00;
    strOUT[2] = 0x00;

#define NCI_MSG_RF_INTF_ACTIVATED 5

    NCI_MSG_BLD_HDR0(strOUT, NCI_MT_NTF, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1(strOUT, NCI_MSG_RF_INTF_ACTIVATED);
}

void vshort_sendata(uint8_t* strIN, int sizLen, uint8_t* strOUT)
{
    int ii;
    int offset = 3;

    for (ii = 0; ii <= sizLen + offset; ii++)
        strOUT[ii + offset] = strIN[ii];

    strOUT[0] = 0x00;
    strOUT[1] = 0x00;
    strOUT[2] = 0x00;

    NCI_DATA_BLD_HDR(strOUT, 0, sizLen);
}

int codeNFC(NfcPayload* nfcData, uint8_t* msg)
{
    int Type = nfcData->type;
    int Lang = nfcData->lang;

    int argLen = strlen(nfcData->text);
    int offsetPrefix = 1;
    int sizehdr = 4;  // header + Prefix
    int msg_len = 0;

    LOGM("codeNFC - %d %d %s %s", nfcData->type, nfcData->lang, nfcData->text, nfcData->tittle);

    switch (Type)
    {
    case 0:
        sizehdr += offsetPrefix;
        msg_len = (argLen + sizehdr);
        // msg = (uint8_t*)calloc(msg_len, sizeof(uint8_t));
        createBufNdef_TypeURI((unsigned char*) nfcData->text, argLen, msg);
        break;

    case 1:  // Type Text need calculate HeaderSize
        offsetPrefix = 6;
        if (Lang == 0)
        {
            // sizehdr += 6 ;
            sizehdr += offsetPrefix;
            msg_len = (argLen + sizehdr);
            // msg = (uint8_t*)calloc(msg_len, sizeof(uint8_t));
            createBufNdef_TypeText((unsigned char*) nfcData->text, argLen, msg);
        }
        else if (Lang == 1)
        {
            // sizehdr += 3 ;
            sizehdr += offsetPrefix;
            msg_len = (argLen + sizehdr);
            // msg = (uint8_t*)calloc(msg_len, sizeof(uint8_t));
            createBufNdef_TypeText((unsigned char*) nfcData->text, argLen, msg);
        }
        break;

    case 2:
        offsetPrefix = 6;
        sizehdr += offsetPrefix;
        msg_len = (argLen + sizehdr) + 11;
        // msg = (uint8_t*)calloc(msg_len, sizeof(uint8_t));
        createBufNdef_TypeSmartPoster((unsigned char*) nfcData->text,
                                      (unsigned char*) nfcData->tittle, argLen, msg);
        break;
    case 3:
        sizehdr = 2;
        msg_len = (argLen + sizehdr);
    // msg = (uint8_t*)calloc(msg_len, sizeof(uint8_t));
    // vshort_sendata(argbuff,  argLen, msg);

    default:
        break;
    }
    return msg_len;
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

/*******************************************************************************
**
** Function         NDEF_MsgValidate
**
** Description      This function validates an NDEF message.
**
** Returns          TRUE if all OK, or FALSE if the message is invalid.
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgValidate(uint8_t* p_msg, uint32_t msg_len, int b_allow_chunks)
{
    uint8_t* p_rec = p_msg;
    uint8_t* p_end = p_msg + msg_len;
    uint8_t rec_hdr = 0, type_len, id_len;
    int count;
    uint32_t payload_len;
    int bInChunk = FALSE;

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

        /* The second and all subsequent records must NOT have the MB bit set */
        if ((count > 0) && (rec_hdr & NDEF_MB_MASK))
            return (NDEF_MSG_EXTRA_MSG_BEGIN);

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
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
        }
        else
            id_len = 0;

        /* A chunk must have type "unchanged", and no type or ID fields */
        if (rec_hdr & NDEF_CF_MASK)
        {
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
            if ((type_len != 0) || (id_len != 0) || (payload_len != 0))
                return (NDEF_MSG_INVALID_EMPTY_REC);
        }

        if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNKNOWN)
        {
            if (type_len != 0)
                return (NDEF_MSG_LENGTH_MISMATCH);
        }

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);

        if (rec_hdr & NDEF_ME_MASK)
            break;

        rec_hdr = 0;
    }

    /* The last record should have the ME bit set */
    if ((rec_hdr & NDEF_ME_MASK) == 0)
        return (NDEF_MSG_NO_MSG_END);

    /* p_rec should equal p_end if all the length fields were correct */
    if (p_rec != p_end)
        return (NDEF_MSG_LENGTH_MISMATCH);

    return (NDEF_OK);
}
