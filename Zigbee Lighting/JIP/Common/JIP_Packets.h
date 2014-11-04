/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          JIP_Packets.h
 *
 * REVISION:           $Revision: 56798 $
 *
 * DATED:              $Date: 2013-09-23 14:53:10 +0100 (Mon, 23 Sep 2013) $
 *
 * AUTHOR:             Matt Redfearn
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the 
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

#ifndef JIP_PACKETS_H_
#define JIP_PACKETS_H_

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

#include <stdint.h>
#include <JIP.h>

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define COMPILE_TIME_ASSERT(NAME, A) \
    typedef uint8_t __u8Assert_ ## NAME [(A) ? 1 : -1]

#define JIP_VERSION 0

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum _eJIP_Command
{
    E_JIP_COMMAND_GET_REQUEST = 0x10,      /* Request to get the value of a variable */
    E_JIP_COMMAND_GET_RESPONSE,            /* Response to a previous get request */

    E_JIP_COMMAND_SET_REQUEST,             /* Request to set the value of a variable */
    E_JIP_COMMAND_SET_RESPONSE,            /* Response to a previous set request */

    E_JIP_COMMAND_QUERY_MIB_REQUEST,       /* Request to query the JIP database for a list of available MIBs */
    E_JIP_COMMAND_QUERY_MIB_RESPONSE,      /* Response to a previous query request */

    E_JIP_COMMAND_QUERY_VAR_REQUEST,       /* Request to query the JIP database for a list of available variables */
    E_JIP_COMMAND_QUERY_VAR_RESPONSE,      /* Response to a previous query request */

    E_JIP_COMMAND_TRAP_REQUEST,            /* Request to generate a notification when a variable changes value */
    E_JIP_COMMAND_UNTRAP_REQUEST,          /* Request to stop generate a notification when a variable changes value */
    E_JIP_COMMAND_TRAP_RESPONSE,           /* Response to a previous trap request*/
    E_JIP_COMMAND_TRAP_NOTIFY,             /* Notify of a change in a previously trapped variable */

    E_JIP_COMMAND_GET_MIB_REQUEST,         /* Request to get the value of a variable using MiB Id */
    E_JIP_COMMAND_SET_MIB_REQUEST,         /* Request to set the value of a variable using MiB Id */

    E_JIP_COMMAND_LAST

} PACK teJIP_Command;

COMPILE_TIME_ASSERT(CheckSizeofJIPVarType,    sizeof(teJIP_VarType)    == 1);
COMPILE_TIME_ASSERT(CheckSizeofJIPAccessType, sizeof(teJIP_AccessType) == 1);
COMPILE_TIME_ASSERT(CheckSizeofJIPSecurity,   sizeof(teJIP_Security)   == 1);

#ifdef WIN32
#pragma pack(push, 1)
#endif

typedef struct
{
	uint8_t                             u8Version;
	teJIP_Command                       eCommand;
	uint8_t                             u8Handle;

#ifndef WIN32
	uint8_t                             au8Payload[0];
#endif
} PACK tsJIP_MsgHeader;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_MsgHeader, sizeof(tsJIP_MsgHeader) == 3);

typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	uint8_t                             u8MibIndex;
	uint8_t                             u8VarIndex;
	teJIP_Status                        eStatus;
} PACK tsJIP_Msg_VarStatus;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarStatus, sizeof(tsJIP_Msg_VarStatus) == 6);


typedef struct
{
    tsJIP_MsgHeader                     sHeader;

    uint8_t                             u8MibIndex;
    uint8_t                             u8VarIndex;
    teJIP_Status                        eStatus;
} PACK tsJIP_Msg_VarDescriptionHeaderError;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescriptionHeaderError,
                    sizeof(tsJIP_Msg_VarDescriptionHeaderError) == 6);


typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	uint8_t                             u8MibIndex;
	uint8_t                             u8VarIndex;
	teJIP_Status                        eStatus;
	teJIP_VarType                       eVarType;

#ifndef WIN32
	uint8_t                             au8Payload[0];
#endif
} PACK tsJIP_Msg_VarDescriptionHeader;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescriptionHeader,
					sizeof(tsJIP_Msg_VarDescriptionHeader) == 7);


typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint32_t                            u32Val;
} PACK tsJIP_Msg_VarDescription_Int32;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Int32,
					sizeof(tsJIP_Msg_VarDescription_Int32) == 11);

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint32_t                            u32Val;
} PACK tsJIP_Msg_VarDescription_Flt;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Flt,
					sizeof(tsJIP_Msg_VarDescription_Flt) == 11);

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint64_t                            u64Val;
} PACK tsJIP_Msg_VarDescription_Int64;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Int64,
					sizeof(tsJIP_Msg_VarDescription_Int64) == 15);

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint64_t                            u64Val;
} PACK tsJIP_Msg_VarDescription_Dbl;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Dbl,
					sizeof(tsJIP_Msg_VarDescription_Dbl) == 15);

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint8_t                             u8StringLen;

#ifndef WIN32
	char                                acString[0]; /* arbitrary length, up to 255 */
#endif
} PACK tsJIP_Msg_VarDescription_Str;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Str,
					sizeof(tsJIP_Msg_VarDescription_Str) == 8);

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint8_t                             u8Val;
} PACK tsJIP_Msg_VarDescription_Int8;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Int8,
					sizeof(tsJIP_Msg_VarDescription_Int8) == 8);

typedef struct
{
    tsJIP_Msg_VarDescriptionHeader      sHeader;

    uint16_t                            u16Val;
} PACK tsJIP_Msg_VarDescription_Int16;
COMPILE_TIME_ASSERT(CheckSizeof_tsJIP_Msg_VarDescription_Int16,
                    sizeof(tsJIP_Msg_VarDescription_Int16) == 9);

typedef struct
{
    teJIP_Status                        eStatus;
} PACK tsJIP_Msg_VarDescriptionEntryError;

typedef struct
{
    teJIP_Status                        eStatus;
    teJIP_VarType                       eVarType;

    uint8_t                             au8Data[0];
} PACK tsJIP_Msg_VarDescriptionEntry;

typedef struct
{
	tsJIP_Msg_VarDescriptionHeader      sHeader;

	uint8_t                             u8Len;
#ifndef WIN32
	uint8_t                             au8Blob[0]; /* arbitrary length, up to 255 */
#endif
} PACK tsJIP_Msg_VarDescription_Blob;

/* JIP Get table response packet */
typedef struct
{
    tsJIP_Msg_VarDescriptionHeader      sHeader;
    
    uint16_t                            u16Remaining;
    uint16_t                            u16TableVersion;
#ifndef WIN32
    uint8_t                             au8Table[0]; /* arbitrary length, up to 255 */
#endif
} PACK tsJIP_Msg_VarDescription_Table;

/* A list of these come after the tsJIP_Msg_VarDescription_Table header */
typedef struct
{
    uint16_t                            u16Entry;
    uint8_t                             u8Len;
#ifndef WIN32
    uint8_t                             au8Blob[0]; /* arbitrary length, up to 255 */
#endif
} PACK tsJIP_Msg_VarDescription_Table_Entry;

typedef struct
{
    uint8_t                             u8VarIndex;

    union {
        uint8_t                         u8VarCount;

        struct 
        {
            uint16_t                    u16FirstEntry;
            uint8_t                     u8EntryCount;
        } PACK;
    } PACK;
} PACK tsJIP_Msg_GetRequest;

/* E_JIP_COMMAND_GET_REQUEST */
typedef struct
{
    tsJIP_MsgHeader                     sHeader;

    uint8_t                             u8MibIndex;

    tsJIP_Msg_GetRequest                sRequest;

} PACK tsJIP_Msg_GetIndexRequest;

/* E_JIP_COMMAND_GET_MIB_REQUEST */
typedef struct
{
    tsJIP_MsgHeader                     sHeader;

    uint32_t                            u32MibId;

    tsJIP_Msg_GetRequest                sRequest;
} PACK tsJIP_Msg_GetMibRequest;

typedef struct
{
    uint8_t                             u8VarIndex;

    tsJIP_Msg_VarDescriptionEntry       sVar;
} PACK tsJIP_Msg_SetRequest;

/* E_JIP_COMMAND_SET_REQUEST */
typedef struct
{
    tsJIP_MsgHeader                     sHeader;

    uint8_t                             u8MibIndex;

    tsJIP_Msg_SetRequest                sRequest;
} PACK tsJIP_Msg_SetIndexRequest;

/* E_JIP_COMMAND_SET_MIB_REQUEST */
typedef struct
{
    tsJIP_MsgHeader                     sHeader;

    uint32_t                            u32MibId;

    tsJIP_Msg_SetRequest                sRequest;
} PACK tsJIP_Msg_SetMibRequest;

/* E_JIP_COMMAND_QUERY_MIB_REQUEST */
typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	uint8_t                             u8MibStartIndex;
	uint8_t                             u8NumMibs;
} PACK tsJIP_Msg_QueryMibRequest;

/* E_JIP_COMMAND_QUERY_MIB_RESPONSE */
typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	teJIP_Status                        eStatus;

	uint8_t                             u8NumMibsReturned;
	uint8_t                             u8NumMibsOutstanding;
} PACK tsJIP_Msg_QueryMibResponseHeader;

typedef struct
{
	uint8_t                             u8MibIndex;
    uint32_t                            u32MibID;
	uint8_t                             u8NameLen;
#ifndef WIN32
	char                                acName[0];
#endif
} PACK tsJIP_Msg_QueryMibResponseListEntryHeader;

/* E_JIP_COMMAND_QUERY_VAR_REQUEST */
typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	uint8_t                             u8MibIndex;

	uint8_t                             u8VarStartIndex;
	uint8_t                             u8NumVars;
} PACK tsJIP_Msg_QueryVarRequest;

/* E_JIP_COMMAND_QUERY_VAR_RESPONSE */
typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	teJIP_Status                        eStatus;

	uint8_t                             u8MibIndex;

	uint8_t                             u8NumVarsReturned;
	uint8_t                             u8NumVarsOutstanding;
} PACK tsJIP_Msg_QueryVarResponseHeader;

typedef struct
{
	uint8_t                             u8VarIndex;
	uint8_t                             u8NameLen;
#ifndef WIN32
	char                                acName[0];
#endif
} PACK tsJIP_Msg_QueryVarResponseListEntryHeader;

typedef struct
{
    teJIP_VarType                       eVarType;
    teJIP_AccessType                    eAccessType;
    teJIP_Security                      eSecurity;
} PACK tsJIP_Msg_QueryVarResponseListEntryFooter;

/* E_JIP_COMMAND_TRAP_REQUEST */
typedef struct
{
	tsJIP_MsgHeader                     sHeader;

	uint8_t                             u8NotificationHandle;
	uint8_t                             u8MibIndex;
	uint8_t                             u8VarIndex;
} PACK tsJIP_Msg_TrapRequest;

typedef struct
{
    tsJIP_MsgHeader                     sHeader;
    uint8_t                             u8Join;
    uint32_t                            u32TreeVersion;
} PACK tsJIP_Msg_TreeVersion;

#ifdef WIN32
#pragma pack(pop)
#endif

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif /*JIP_PACKETS_H_*/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
