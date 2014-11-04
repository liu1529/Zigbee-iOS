/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Persist.c
 *
 * REVISION:           $Revision: 54378 $
 *
 * DATED:              $Date: 2013-05-30 11:23:01 +0100 (Thu, 30 May 2013) $
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h> 

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>


#include <JIP.h>
#include <JIP_Private.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_PERSIST 0

#define CACHE_FILE_VERSION 3

#define MY_ENCODING "ISO-8859-1"


teJIP_Status eJIPService_PersistXMLSaveDefinitions(tsJIP_Context *psJIP_Context, const char *pcFileName)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsMibIDCacheEntry       *psMibCacheEntry;
    tsDeviceIDCacheEntry    *psDeviceCacheEntry;
    tsNode *psNode;
    tsMib *psMib;
    tsVar *psVar;
    int rc;
    xmlTextWriterPtr writer;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    LIBXML_TEST_VERSION

    DBG_vPrintf(DBG_PERSIST, "Saving network state to %s\n", pcFileName);

    /* Create a new XmlWriter for uri, with no compression. */
    writer = xmlNewTextWriterFilename(pcFileName, 0);
    if (writer == NULL) {
        printf("testXmlwriterFilename: Error creating the xml writer\n");
        return E_JIP_ERROR_FAILED;
    }
    
    xmlTextWriterSetIndent(writer, 1);

    /* Start the document with the xml default for the version,
     * encoding ISO 8859-1 and the default for the standalone
     * declaration. */
    rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartDocument\n");
        return E_JIP_ERROR_FAILED;
    }

    /* Start an element named "JIP_Cache" */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "JIP_Cache");
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
        return E_JIP_ERROR_FAILED;
    }
    
    /* Add an attribute with name "Version" */
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Version",
                                            "%d", CACHE_FILE_VERSION);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
        return E_JIP_ERROR_FAILED;
    }
    
    /* Start an element named "MibIdCache" */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "MibIdCache");
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
        return E_JIP_ERROR_FAILED;
    }
    
    psMibCacheEntry = psJIP_Private->sCache.psMibCacheHead;
    DBG_vPrintf(DBG_PERSIST, "Cache head: %p\n", psMibCacheEntry);
    while (psMibCacheEntry)
    {
        psMib = psMibCacheEntry->psMib;
        
        DBG_vPrintf(DBG_PERSIST, "Saving Mib id 0x%08x into Mib cache\n", psMib->u32MibId);
        
        /* Start an element named "Mib" as child of MibIDCache. */
        rc = xmlTextWriterStartElement(writer, BAD_CAST "Mib");
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Add an attribute with name "MibID" */
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ID",
                                            "0x%08x", psMib->u32MibId);
        if (rc < 0) {
            printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
            return E_JIP_ERROR_FAILED;
        }
        
        psVar = psMib->psVars;
        while (psVar)
        {
            /* Start an element named "Var" as child of "Mib". */
            rc = xmlTextWriterStartElement(writer, BAD_CAST "Var");
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
                return E_JIP_ERROR_FAILED;
            }

            /* Add an attribute with name "Index" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Index",
                                                "%02d", psVar->u8Index);
            if (rc < 0) {
                printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Add an attribute with name "Name" */
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "Name",
                                            BAD_CAST psVar->pcName);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
                return E_JIP_ERROR_FAILED;
            }

            /* Add an attribute with name "Type" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Type",
                                                "%02d", psVar->eVarType);
            if (rc < 0) {
                printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
                return E_JIP_ERROR_FAILED;
            }

            /* Add an attribute with name "Access" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Access",
                                                "%02d", psVar->eAccessType);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
                return E_JIP_ERROR_FAILED;
            }
        
            /* Add an attribute with name "Security" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Security",
                                                "%02d", psVar->eSecurity);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
                return E_JIP_ERROR_FAILED;
            }

            
            /* Don't write out the value, we dont really want to cache that do we?? */
            
            /* Close the element named "Var". */
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Next variable */
            psVar = psVar->psNext;
        }
        
        /* Close the element named "Mib". */
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Next entry */
        psMibCacheEntry = psMibCacheEntry->psNext;
    }
    
    /* Close the element named "MibIdCache". */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
        return E_JIP_ERROR_FAILED;
    }

    /* Start an element named "DeviceIdCache" */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "DeviceIdCache");
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
        return E_JIP_ERROR_FAILED;
    }
    
    psDeviceCacheEntry = psJIP_Private->sCache.psDeviceCacheHead;
    DBG_vPrintf(DBG_PERSIST, "Cache head: %p\n", psDeviceCacheEntry);
    while (psDeviceCacheEntry)
    {
        psNode = psDeviceCacheEntry->psNode;
        
        DBG_vPrintf(DBG_PERSIST, "Saving device id 0x%08x into device ID cache\n", psNode->u32DeviceId);
        
        /* Start an element named "Device" as child of DeviceIdCache. */
        rc = xmlTextWriterStartElement(writer, BAD_CAST "Device");
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Add an attribute with name "Device_Id" */
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ID",
                                            "0x%08x", psNode->u32DeviceId);
        if (rc < 0) {
            printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
            return E_JIP_ERROR_FAILED;
        }
 
        psMib = psNode->psMibs;
        while (psMib)
        {
            /* Start an element named "Mib" as child of Device. */
            rc = xmlTextWriterStartElement(writer, BAD_CAST "Mib");
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Add an attribute with name "ID" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ID",
                                                "0x%08x", psMib->u32MibId);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Add an attribute with name "Index" */
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Index",
                                                "%02d", psMib->u8Index);
            if (rc < 0) {
                printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Add an attribute with name "Name" */
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "Name",
                                            BAD_CAST psMib->pcName);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Close the element named "MiB". */
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
                return E_JIP_ERROR_FAILED;
            }
            
            /* Next Mib */
            psMib = psMib->psNext;
        }

        /* Close the element named "Device". */
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Next entry */
        psDeviceCacheEntry = psDeviceCacheEntry->psNext;
    }
    
    /* Finish the document and write it out */
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterEndDocument\n");
        return E_JIP_ERROR_FAILED;
    }

    /* Free up it's memory */
    xmlFreeTextWriter(writer);
    
    xmlCleanupParser();

    return E_JIP_OK;
}


teJIP_Status eJIPService_PersistXMLSaveNetwork(tsJIP_Context *psJIP_Context, const char *pcFileName)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsNode *psNode;
    char acNodeAddressBuffer[INET6_ADDRSTRLEN] = "Could not determine address\n";
    int rc;
    xmlTextWriterPtr writer;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    LIBXML_TEST_VERSION

    DBG_vPrintf(DBG_PERSIST, "Saving network state to %s\n", pcFileName);

    /* Create a new XmlWriter for uri, with no compression. */
    writer = xmlNewTextWriterFilename(pcFileName, 0);
    if (writer == NULL) {
        printf("testXmlwriterFilename: Error creating the xml writer\n");
        return E_JIP_ERROR_FAILED;
    }
    
    xmlTextWriterSetIndent(writer, 1);

    /* Start the document with the xml default for the version,
     * encoding ISO 8859-1 and the default for the standalone
     * declaration. */
    rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartDocument\n");
        return E_JIP_ERROR_FAILED;
    }

    /* Start an element named "JIP_Cache" */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "JIP_Cache");
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
        return E_JIP_ERROR_FAILED;
    }
    
    /* Add an attribute with name "Version" */
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "Version",
                                            "%d", CACHE_FILE_VERSION);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
        return E_JIP_ERROR_FAILED;
    }
    

    /* Start an element named "Network" */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "Network");
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
        return E_JIP_ERROR_FAILED;
    }
    
    inet_ntop(AF_INET6, &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address.sin6_addr, acNodeAddressBuffer, INET6_ADDRSTRLEN);
    
    /* Add an attribute with name "BorderRouter" */
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "BorderRouter",
                                        BAD_CAST acNodeAddressBuffer);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
        return E_JIP_ERROR_FAILED;
    }

    psNode = psJIP_Context->sNetwork.psNodes;
    while (psNode)
    {
        /* Start an element named "Node" as child of Network. */
        rc = xmlTextWriterStartElement(writer, BAD_CAST "Node");
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterStartElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Add an attribute with name "Device_Id" */
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "DeviceID",
                                            "0x%08x", psNode->u32DeviceId);
        if (rc < 0) {
            printf ("testXmlwriterFilename: Error at xmlTextWriterWriteFormatAttribute\n");
            return E_JIP_ERROR_FAILED;
        }
        
        inet_ntop(AF_INET6, &psNode->sNode_Address.sin6_addr, acNodeAddressBuffer, INET6_ADDRSTRLEN);

        /* Add an attribute with name "Address" */
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "Address",
                                        BAD_CAST acNodeAddressBuffer);
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterWriteAttribute\n");
            return E_JIP_ERROR_FAILED;
        }

        /* Close the element named "Node". */
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("testXmlwriterFilename: Error at xmlTextWriterEndElement\n");
            return E_JIP_ERROR_FAILED;
        }
        
        /* Next node */
        psNode = psNode->psNext;
    }
        
    /* Finish the document and write it out */
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        printf("testXmlwriterFilename: Error at xmlTextWriterEndDocument\n");
        return E_JIP_ERROR_FAILED;
    }

    /* Free up it's memory */
    xmlFreeTextWriter(writer);
    
    xmlCleanupParser();

    return E_JIP_OK;
}


static     tsJIP_Context *psDecodeJipContext;
static     int iValidCacheFile          = 0;
static     int iPopulateNetworkNodes    = 1; // Populate the network from file if possible
static     int iPendingMib              = 0;
static     int iPendingDeviceIdNode     = 0;
static     tsNode *psNode = NULL;
static     tsMib *psMib;
static     tsVar *psVar;


/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static void
processNode(xmlTextReaderPtr reader) {

    PRIVATE_CONTEXT(psDecodeJipContext);

    static enum
    {
        E_STATE_NONE,
        E_STATE_MIBS,
        E_STATE_DEVICES,
        E_STATE_NETWORK,
    } eState = E_STATE_NONE;
    
    char *pcBorderRouterAddress = NULL;
    char *NodeName              = NULL;
    char *pcVersion             = NULL;
    char *pcMibId               = NULL;
    char *pcName                = NULL;
    char *pcIndex               = NULL;
    char *pcType                = NULL;
    char *pcAccess              = NULL;
    char *pcSecurity            = NULL;
    char *pcDeviceId            = NULL;
    char *pcAddress             = NULL;


    NodeName = (char *)xmlTextReaderName(reader);
    
    if (strcmp(NodeName, "JIP_Cache") == 0)
    {
        if (xmlTextReaderAttributeCount(reader) == 1)
        {
            long int u32Version;
            
            pcVersion = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Version");
            if (pcVersion == NULL)
            {
                fprintf(stderr, "No Version attribute for Cache file\n");
                u32Version = 0;
                goto done;
            }
            else
            {
                errno = 0;
                u32Version = strtoll(pcVersion, NULL, 16);
                if (errno)
                {
                    u32Version = 0;
                    perror("strtol");
                goto done;
                }
            }
            
            if (u32Version == CACHE_FILE_VERSION)
            {
                DBG_vPrintf(DBG_PERSIST, "Valid cache file\n");
                iValidCacheFile = 1;
                goto done;
            }
            else
            {
                DBG_vPrintf(DBG_PERSIST, "Incorrect version of cache file (%ld) for this version of libJIP(%d)\n", u32Version, CACHE_FILE_VERSION);
                goto done;
            }
        }
    }
    
    if (!iValidCacheFile)
    {
        DBG_vPrintf(DBG_PERSIST, "Not a valid cache file\n");
        /* Not a valid cache file */
        goto done;
    }

    if (strcmp(NodeName, "MibIdCache") == 0)
    {
        eState = E_STATE_MIBS;
        goto done;
    }
    else if (strcmp(NodeName, "DeviceIdCache") == 0)
    {
        eState = E_STATE_DEVICES;
        if (iPendingMib && psMib)
        {
            DBG_vPrintf(DBG_PERSIST, "Adding Mib ID 0x%08x to cache\n", psMib->u32MibId);
            (void)Cache_Add_Mib(&psJIP_Private->sCache, psMib);
            eJIP_FreeMib(psDecodeJipContext, psMib);
            psMib = NULL;
            iPendingMib = 0;
        }
        goto done;
    }
    else if (strcmp(NodeName, "Network") == 0)
    {
        tsJIPAddress sBorder_Router_IPv6_Address;
    
        pcBorderRouterAddress = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"BorderRouter");
        if (pcBorderRouterAddress == NULL)
        {
            fprintf(stderr, "No border router attribute for Cache file\n");
            goto done;
        }
        
        /* Get border router IPv6 address */
        if (inet_pton(AF_INET6, pcBorderRouterAddress, &sBorder_Router_IPv6_Address.sin6_addr) <= 0)
        {
            DBG_vPrintf(DBG_PERSIST, "Could not determine network address from [%s]\n", pcBorderRouterAddress);
            goto done;
        }
        
        if (memcmp(&sBorder_Router_IPv6_Address.sin6_addr, &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address.sin6_addr, sizeof(struct in6_addr)) != 0)
        {
            DBG_vPrintf(DBG_PERSIST, "Cache file for different border router\n");
            iValidCacheFile = 0;
            goto done;
        }
        eState = E_STATE_NETWORK;
        goto done;
    }
    
    switch (eState)
    {
        case(E_STATE_MIBS):
        {
            if (strcmp(NodeName, "Mib") == 0)
            {
                if (xmlTextReaderAttributeCount(reader) == 1)
                {
                    uint32_t u32MibId;
                    
                    /* Add previous Mib */
                    if (iPendingMib && psMib)
                    {
                        DBG_vPrintf(DBG_PERSIST, "Adding Mib ID 0x%08x to cache\n", psMib->u32MibId);
                        (void)Cache_Add_Mib(&psJIP_Private->sCache, psMib);
                        eJIP_FreeMib(psDecodeJipContext, psMib);
                        psMib = NULL;
                        iPendingMib = 0;
                    }

                    pcMibId = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"ID");
                    if (pcMibId == NULL)
                    {
                        fprintf(stderr, "No ID attribute for Mib\n");
                        goto done;
                    }
                    errno = 0;
                    u32MibId = strtoll(pcMibId, NULL, 16);
                    if (errno)
                    {
                        perror("strtol");
                        goto done;
                    }
                    
                    DBG_vPrintf(DBG_PERSIST, "Got Mib ID 0x%08x\n", u32MibId);

                    psMib = malloc(sizeof(tsMib));
    
                    if (!psMib)
                    {
                        DBG_vPrintf(DBG_PERSIST, "Error allocating space for Mib\n");
                        goto done;
                    }
                    
                    iPendingMib = 1;
                    memset(psMib, 0, sizeof(tsMib));
                    psMib->u32MibId = u32MibId;
                }
            }
            else if (strcmp(NodeName, "Var") == 0)
            {
                if (xmlTextReaderAttributeCount(reader) == 5)
                {
                    uint8_t u8Index;
                    pcName      = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Name");
                    if (pcName == NULL)
                    {
                        fprintf(stderr, "No Name attribute for Var\n");
                        goto done;
                    }
                    pcIndex     = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Index");
                    if (pcIndex == NULL)
                    {
                        fprintf(stderr, "No Index attribute for Var\n");
                        goto done;
                    }
                    pcType      = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Type");
                    if (pcType == NULL)
                    {
                        fprintf(stderr, "No Type attribute for Var\n");
                        goto done;
                    }
                    pcAccess    = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Access");
                    if (pcAccess == NULL)
                    {
                        fprintf(stderr, "No Access Type attribute for Var\n");
                        goto done;
                    }
                    pcSecurity  = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Security");
                    if (pcSecurity == NULL)
                    {
                        fprintf(stderr, "No Security attribute for Var\n");
                        goto done;
                    }
                    u8Index = atoi(pcIndex);
                    DBG_vPrintf(DBG_PERSIST, "Got Var index %d with name %s, Type %s, Access %s, Security %s\n", u8Index, pcName, pcType, pcAccess, pcSecurity);

                    psVar = psJIP_MibAddVar(psMib, u8Index, pcName, atoi(pcType), atoi(pcAccess), atoi(pcSecurity));
                }
            }
            break;
        }
        
        case(E_STATE_DEVICES):
        {
            if (strcmp(NodeName, "Device") == 0)
            {
                if (xmlTextReaderAttributeCount(reader) == 1)
                {
                    long int u32DeviceId;
                    
                    /* Add previous entry */
                    if (iPendingDeviceIdNode && psNode)
                    {
                        DBG_vPrintf(DBG_PERSIST, "Adding Device ID 0x%08x to cache\n", psNode->u32DeviceId);
                        Cache_Add_Node(&psJIP_Private->sCache, psNode);
                        eJIP_NetFreeNode(psDecodeJipContext, psNode);
                        psNode = NULL;
                        iPendingDeviceIdNode = 0;
                    }
                    
                    pcDeviceId = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"ID");
                    if (pcDeviceId == NULL)
                    {
                        fprintf(stderr, "No ID attribute for Device\n");
                        goto done;
                    }
                    else
                    {
                        errno = 0;
                        u32DeviceId = strtoll(pcDeviceId, NULL, 16);
                        if (errno)
                        {
                            perror("strtol");
                            goto done;
                        }
                    }

                    DBG_vPrintf(DBG_PERSIST, "Got device ID 0x%08x\n", (unsigned int)u32DeviceId);

                    DBG_vPrintf(DBG_PERSIST, "Creating new node for device cache\n");
                    psNode = malloc(sizeof(tsNode));

                    if (!psNode)
                    {
                        DBG_vPrintf(DBG_PERSIST, "Error allocating space for Node\n");
                        goto done;
                    }
                    
                    iPendingDeviceIdNode = 1;

                    memset(psNode, 0, sizeof(tsNode));

                    psNode->u32DeviceId    = u32DeviceId;
                    psNode->u32NumMibs     = 0;
                    
                    eLockCreate(&psNode->sLock);
                }
            }
            else if (strcmp(NodeName, "Mib") == 0)
            {
                if (xmlTextReaderAttributeCount(reader) == 3)
                {
                    uint32_t u32MibId;
                    uint8_t u8Index;
                    
                    pcMibId = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"ID");
                    if (pcMibId == NULL)
                    {
                        fprintf(stderr, "No ID attribute for Mib\n");
                        goto done;
                    }
                    else
                    {
                        errno = 0;
                        u32MibId = strtoll(pcMibId, NULL, 16);
                        if (errno)
                        {
                            perror("strtol");
                            goto done;
                        }
                    }
                    
                    pcIndex = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Index");
                    if (pcIndex == NULL)
                    {
                        fprintf(stderr, "No Index attribute for Mib\n");
                        goto done;
                    }
                    u8Index = atoi(pcIndex);
                    
                    pcName = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Name");
                    if (pcName == NULL)
                    {
                        fprintf(stderr, "No Name attribute for Mib\n");
                        goto done;
                    }

                    DBG_vPrintf(DBG_PERSIST, "Got Mib ID 0x%0x Index %d with name %s\n", u32MibId, u8Index, pcName);
                    
                    /* Add the Mib to the device cache */
                    psMib = psJIP_NodeAddMib(psNode, u32MibId, u8Index, pcName);
                    
                    /* And populate it from the vars we already loaded */
                    if (Cache_Populate_Mib(&psJIP_Private->sCache, psMib) != E_JIP_OK)
                    {
                        DBG_vPrintf(DBG_PERSIST, "Failed to populate Mib ID 0x%0x from cache\n", u32MibId);
                    }
                }
            }
            break;
        }
        
        case(E_STATE_NETWORK):
        {
            if (strcmp(NodeName, "Node") == 0)
            {
                if (xmlTextReaderAttributeCount(reader) == 2)
                {
                    struct sockaddr_in6 sNodeAddress;

                    uint32_t u32DeviceId;
                    int s;
                    
                    pcDeviceId = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"DeviceID");
                    if (pcDeviceId == NULL)
                    {
                        fprintf(stderr, "No DeviceID attribute for Node\n");
                        goto done;
                    }
                    else
                    {
                        errno = 0;
                        u32DeviceId = strtoll(pcDeviceId, NULL, 16);
                        if (errno)
                        {
                            perror("strtol");
                            goto done;
                        }
                    }

                    pcAddress = (char *)xmlTextReaderGetAttribute(reader, (unsigned char *)"Address");
                    if (pcAddress == NULL)
                    {
                        fprintf(stderr, "No Address attribute for Node\n");
                        goto done;
                    }

                    DBG_vPrintf(DBG_PERSIST, "Got node device type 0x%08x with address %s\n", u32DeviceId, pcAddress);

                    memset (&sNodeAddress, 0, sizeof(struct sockaddr_in6));
                    sNodeAddress.sin6_family  = AF_INET6;
                    sNodeAddress.sin6_port    = htons(JIP_DEFAULT_PORT);

                    s = inet_pton(AF_INET6, pcAddress, &sNodeAddress.sin6_addr);
                    if (s <= 0)
                    {
                        if (s == 0)
                        {
                            fprintf(stderr, "Unknown host: %s\n", pcAddress);
                        }
                        else if (s < 0)
                        {
                            perror("inet_pton failed");
                        }
                        goto done;
                    }

                    if (iPopulateNetworkNodes)
                    {

                        DBG_vPrintf(DBG_PERSIST, "Adding node device type 0x%08x with address %s to network\n", u32DeviceId, pcAddress);
                        eJIP_NetAddNode(psDecodeJipContext, &sNodeAddress, u32DeviceId, NULL);
                    }
                }
            }
            break;
        }
        default:
            DBG_vPrintf(DBG_PERSIST, "In unknown state\n");
            break;
    }

done:
    free(pcBorderRouterAddress);
    free(NodeName);
    free(pcVersion);
    free(pcMibId);
    free(pcName);
    free(pcIndex);
    free(pcType);
    free(pcAccess);
    free(pcSecurity);
    free(pcDeviceId);
    free(pcAddress);
    
    return;
}


static teJIP_Status eJIP_PersistLoad(tsJIP_Context *psJIP_Context, const char *pcFileName, const int iPopulateNetwork)
{
    PRIVATE_CONTEXT(psJIP_Context);
    int ret;
    xmlTextReaderPtr reader;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    LIBXML_TEST_VERSION

    DBG_vPrintf(DBG_PERSIST, "Loading network state from %s\n", pcFileName);
    
    iPopulateNetworkNodes = iPopulateNetwork;
    
    DBG_vPrintf(DBG_PERSIST, "Populating Device Cache\n"); // Always done
    if (iPopulateNetworkNodes)
    {
        DBG_vPrintf(DBG_PERSIST, "Populating Nodes\n");
    }
    
    psDecodeJipContext = psJIP_Context;
    
    reader = xmlReaderForFile(pcFileName, NULL, 0);
    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            processNode(reader);
            ret = xmlTextReaderRead(reader);
        }
        
        /* Finish up */
        if (iPendingDeviceIdNode && psNode)
        {
            DBG_vPrintf(DBG_PERSIST, "Adding Device ID 0x%08x to cache\n", psNode->u32DeviceId);
            Cache_Add_Node(&psJIP_Private->sCache, psNode);
            eJIP_NetFreeNode(psDecodeJipContext, psNode);
            psNode = NULL;
            iPendingDeviceIdNode = 0;
        }
        
        xmlTextReaderClose(reader);
        xmlFreeTextReader(reader);
        xmlCleanupParser();
        
        if (ret != 0) {
            fprintf(stderr, "%s : failed to parse\n", pcFileName);
            return E_JIP_ERROR_FAILED;
        }
    } else {
        fprintf(stderr, "Unable to open %s\n", pcFileName);
        return E_JIP_ERROR_FAILED;
    }
    
    if (!iValidCacheFile)
    {
        /* Wasn't a valid cache file */
        return E_JIP_ERROR_FAILED;
    }  

    return E_JIP_OK;
}


teJIP_Status eJIPService_PersistXMLLoadDefinitions(tsJIP_Context *psJIP_Context, const char *pcFileName)
{
    return eJIP_PersistLoad(psJIP_Context, pcFileName, 0);
}

teJIP_Status eJIPService_PersistXMLLoadNetwork(tsJIP_Context *psJIP_Context, const char *pcFileName)
{
    return eJIP_PersistLoad(psJIP_Context, pcFileName, 1);
}

