#include <Windows.h>
#include <msxml6.h>
#include <atlcomcli.h>
#include "ORADAD.h"

extern HANDLE g_hHeap;

//
// Private functions
//
BOOL
pReadAttributes(
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPCWSTR szXPath,
   _Outptr_ PDWORD dwAttributesCount,
   _Outptr_ PATTRIBUTE_CONFIG *pAttributes
);

BOOL
pReadAttributeString(
   _In_ IXMLDOMNamedNodeMap *pXmlAttributeMap,
   _In_z_ LPWSTR szAttributeName,
   _Out_ LPWSTR *szValue
);

BOOL
pReadAttributeInterger(
   _In_ IXMLDOMNamedNodeMap *pXmlAttributeMap,
   _In_z_ LPWSTR szAttributeName,
   _Out_ PDWORD pdwValue
);

BOOL
pAddClassAttributes(
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPWSTR szClassName,
   _In_z_ PCLASS_CONFIG pClass,
   _In_ DWORD dwAttributesCount,
   _In_ PATTRIBUTE_CONFIG pAttributes
);

BOOL
pAddClassesToRequest(
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPWSTR szClassName,
   PREQUEST_CONFIG pRequest,
   _In_ PGLOBAL_CONFIG pGlobalConfig
);

PATTRIBUTE_CONFIG
pFindAttribute(
   _In_ DWORD dwAttributesCount,
   _In_ PATTRIBUTE_CONFIG pAttributes,
   _In_z_ LPWSTR szName
);

BOOL
pXmlParseRequest(
   _In_ IXMLDOMDocument2 *pXMLDoc,
   IXMLDOMNode *pXmlNodeRequet,
   PREQUEST_CONFIG pRequests,
   _In_ PGLOBAL_CONFIG pGlobalConfig
);

DWORD
pReadUInteger(
   _In_opt_z_ LPCWSTR szValue
);

BOOL
pReadBoolean(
   _In_opt_z_ LPCWSTR szValue
);

//
// Public functions
//
// Note: we return PVOID to avoid include msxml.h in all files.
PVOID
XmlReadConfigFile (
   _In_z_ LPTSTR szConfigPath,
   _In_ PGLOBAL_CONFIG pGlobalConfig
)
{
   BOOL bResult;
   HRESULT hr;
   VARIANT_BOOL bSuccess = false;

   IXMLDOMDocument2 *pXMLDoc = NULL;
   IXMLDOMNode *pXMLNode = NULL;
   IXMLDOMNodeList *pXMLNodeList = NULL;

   long lLength;

   Log(
      __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_VERBOSE,
      "Read config file."
   );

   hr = CoCreateInstance(CLSID_FreeThreadedDOMDocument60, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void**)&pXMLDoc);
   if ((hr != S_OK) || (pXMLDoc == NULL))
   {
      Log(
         __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
         "Unable to create XML object (error 0x%08x).", hr
      );
      return NULL;
   }

   hr = pXMLDoc->put_async(VARIANT_FALSE);
   //hr = pXMLDoc->setProperty(L"SelectionLanguage", L"XPath");

   //
   // Load file
   //
   hr = pXMLDoc->load(CComVariant(szConfigPath), &bSuccess);

   if ((hr != S_OK) || (bSuccess == FALSE))
   {
      IXMLDOMParseError *pXmlParseError = NULL;
      BSTR strError;
      LPSTR szError;

      hr = pXMLDoc->get_parseError(&pXmlParseError);
      hr = pXmlParseError->get_reason(&strError);

      RemoveSpecialChars(strError);
      szError = LPWSTRtoLPSTR(strError);

      if (szError != NULL)
      {
         Log(
            __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
            "Unable to parse XML (%s).", szError
         );
         _SafeHeapRelease(szError);
      }

      _SafeCOMRelease(pXmlParseError);
      _SafeCOMRelease(pXMLDoc);
      return NULL;
   }

   //
   // Read Main Config
   //
   hr = pXMLDoc->selectSingleNode((BSTR)TEXT("/configORADAD/config"), &pXMLNode);
   hr = pXMLNode->get_childNodes(&pXMLNodeList);
   hr = pXMLNodeList->get_length(&lLength);

   for (long i = 0; i < lLength; i++)
   {
      IXMLDOMNode *pXmlNodeConfig = NULL;
      BSTR strNodeName;
      BSTR strNodeText;

      hr = pXMLNodeList->get_item(i, &pXmlNodeConfig);
      hr = pXmlNodeConfig->get_nodeName(&strNodeName);
      hr = pXmlNodeConfig->get_text(&strNodeText);

      if ((wcscmp(strNodeName, L"server") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->szServer = strNodeText;
      if ((wcscmp(strNodeName, L"port") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->ulLdapPort = pReadUInteger(strNodeText);
      else if ((wcscmp(strNodeName, L"username") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->szUsername = strNodeText;
      else if ((wcscmp(strNodeName, L"userdomain") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->szUserDomain = strNodeText;
      else if ((wcscmp(strNodeName, L"userpassword") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->szUserPassword = strNodeText;
      else if ((wcscmp(strNodeName, L"allDomainsInForest") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->bAllDomainsInForest = pReadBoolean(strNodeText);
      else if ((wcscmp(strNodeName, L"forestDomains") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->szForestDomains = strNodeText;
      else if ((wcscmp(strNodeName, L"level") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->dwLevel = pReadUInteger(strNodeText);
      else if ((wcscmp(strNodeName, L"sleepTime") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->dwSleepTime = pReadUInteger(strNodeText);
      else if ((wcscmp(strNodeName, L"writeHeader") == 0) && (wcslen(strNodeText) > 0))
         pGlobalConfig->bWriteHeader = pReadBoolean(strNodeText);

      _SafeCOMRelease(pXmlNodeConfig);
   }

   _SafeCOMRelease(pXMLNodeList);
   _SafeCOMRelease(pXMLNode);


   //
   // Check attributes
   //
   // Find all domains in forest can only be done with DC locator option
   if ((wcscmp(pGlobalConfig->szServer, DC_LOCATOR_OPTION) != 0) && (pGlobalConfig->bAllDomainsInForest == TRUE))
   {
      pGlobalConfig->bAllDomainsInForest = FALSE;
   }

   //
   // Read Attributes
   //
   bResult = pReadAttributes(pXMLDoc, L"/configORADAD/schema/rootDSEAttributes/attribute", &pGlobalConfig->dwRootDSEAttributesCount, &pGlobalConfig->pRootDSEAttributes);
   if (bResult == FALSE)
      return NULL;
   bResult = pReadAttributes(pXMLDoc, L"/configORADAD/schema/attributes/attribute", &pGlobalConfig->dwAttributesCount, &pGlobalConfig->pAttributes);
   if (bResult == FALSE)
      return NULL;

   //
   // Read Requests
   //
   hr = pXMLDoc->selectNodes((BSTR)TEXT("/configORADAD/requests/request"), &pXMLNodeList);
   hr = pXMLNodeList->get_length(&lLength);

   pGlobalConfig->dwRequestCount = lLength;
   pGlobalConfig->pRequests = (PREQUEST_CONFIG)_HeapAlloc(lLength * sizeof(REQUEST_CONFIG));

   for (long i = 0; i < lLength; i++)
   {
      BOOL bResult;

      IXMLDOMNode *pXmlNodeRequet = NULL;
      IXMLDOMNodeList *pXmlNodeListRequet = NULL;

      hr = pXMLNodeList->get_item(i, &pXmlNodeRequet);

      bResult = pXmlParseRequest(pXMLDoc, pXmlNodeRequet, &pGlobalConfig->pRequests[i], pGlobalConfig);
      if (bResult == FALSE)
         return NULL;

      if (pGlobalConfig->pRequests[i].dwBase & BASE_ROOTDSE)
      {
         // RootDSE can only be RootDSE. Disable other types
         pGlobalConfig->pRequests[i].dwBase = BASE_ROOTDSE;

         // Allocate per request max attribute text size
         pGlobalConfig->pRequests[i].pdwStrintMaxLength = (PDWORD)_HeapAlloc(sizeof(DWORD) * pGlobalConfig->dwRootDSEAttributesCount);
      }
      else
      {
         // Allocate per request max attribute text size
         pGlobalConfig->pRequests[i].pdwStrintMaxLength = (PDWORD)_HeapAlloc(sizeof(DWORD) * pGlobalConfig->pRequests[i].dwAttributesCount);
      }

      // Free COM
      _SafeCOMRelease(pXmlNodeListRequet);
      _SafeCOMRelease(pXmlNodeRequet);
   }

   _SafeCOMRelease(pXMLNodeList);

   //
   // Display requests and attributes for debug
   //
   /*
   for (DWORD i = 0; i < pGlobalConfig->dwRequestCount; i++)
   {
      wprintf_s(L"%s\n", pGlobalConfig->pRequests[i].szName);
      for (DWORD j = 0; j < pGlobalConfig->pRequests[i].dwAttributesCount; j++)
      {
         wprintf_s(L"   %s\n", pGlobalConfig->pRequests[i].pAttributes[j]->szName);
      }
      wprintf_s(L"\n");
   }
   */

   return pXMLDoc;
}

//
// Private functions
//
BOOL
pReadAttributes (
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPCWSTR szXPath,
   _Outptr_ PDWORD dwAttributesCount,
   _Outptr_ PATTRIBUTE_CONFIG *pAttributes
)
{
   HRESULT hr;

   IXMLDOMNodeList *pXMLNodeList = NULL;

   long lLength;

   hr = pXMLDoc->selectNodes((BSTR)szXPath, &pXMLNodeList);

   hr = pXMLNodeList->get_length(&lLength);

   *dwAttributesCount = lLength;
   *pAttributes = (PATTRIBUTE_CONFIG)_HeapAlloc(lLength * sizeof(ATTRIBUTE_CONFIG));

   for (long i = 0; i < lLength; i++)
   {
      LPWSTR szType;
      LPWSTR szFilter;

      IXMLDOMNode *pXmlNodeAttribute = NULL;
      IXMLDOMNamedNodeMap *pXmlAttributeMap = NULL;

      hr = pXMLNodeList->get_item(i, &pXmlNodeAttribute);
      hr = pXmlNodeAttribute->get_attributes(&pXmlAttributeMap);

      pReadAttributeString(pXmlAttributeMap, (LPWSTR)L"name", &(*pAttributes)[i].szName);
      pReadAttributeInterger(pXmlAttributeMap, (LPWSTR)L"level", &(*pAttributes)[i].dwLevel);
      pReadAttributeString(pXmlAttributeMap, (LPWSTR)L"type", &szType);
      pReadAttributeString(pXmlAttributeMap, (LPWSTR)L"filter", &szFilter);

      if (szFilter != NULL)
      {
         BOOL bResult;

         bResult = GetFilter(&(*pAttributes)[i], szFilter);
         if (bResult == FALSE)
            return FALSE;
      }

      if (_wcsicmp(szType, L"STR") == 0)
         (*pAttributes)[i].Type = TYPE_STR;
      else if (_wcsicmp(szType, L"STRS") == 0)
         (*pAttributes)[i].Type = TYPE_STRS;
      else if (_wcsicmp(szType, L"SID") == 0)
         (*pAttributes)[i].Type = TYPE_SID;
      else if (_wcsicmp(szType, L"SD") == 0)
         (*pAttributes)[i].Type = TYPE_SD;
      else if (_wcsicmp(szType, L"DACL") == 0)
         (*pAttributes)[i].Type = TYPE_DACL;
      else if (_wcsicmp(szType, L"GUID") == 0)
         (*pAttributes)[i].Type = TYPE_GUID;
      else if (_wcsicmp(szType, L"DATE") == 0)
         (*pAttributes)[i].Type = TYPE_DATE;
      else if (_wcsicmp(szType, L"DATEINT64") == 0)
         (*pAttributes)[i].Type = TYPE_DATEINT64;
      else if (_wcsicmp(szType, L"INT") == 0)
         (*pAttributes)[i].Type = TYPE_INT;
      else if (_wcsicmp(szType, L"INT64") == 0)
         (*pAttributes)[i].Type = TYPE_INT64;
      else if (_wcsicmp(szType, L"BOOL") == 0)
         (*pAttributes)[i].Type = TYPE_BOOL;
      else if (_wcsicmp(szType, L"BIN") == 0)
         (*pAttributes)[i].Type = TYPE_BIN;
      else
      {
         Log(
            __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
            "Unknown type (%S).", szType
         );
         return FALSE;
      }

      _SafeCOMRelease(pXmlAttributeMap);
      _SafeCOMRelease(pXmlNodeAttribute);
   }

   _SafeCOMRelease(pXMLNodeList);

   return TRUE;
}

BOOL
pReadAttributeString (
   _In_ IXMLDOMNamedNodeMap *pXmlAttributeMap,
   _In_z_ LPWSTR szAttributeName,
   _Out_ LPWSTR *szValue
)
{
   HRESULT hr;
   IXMLDOMNode *pXmlNodeAttribute = NULL;
   BSTR strName;

   hr = pXmlAttributeMap->getNamedItem((BSTR)szAttributeName, &pXmlNodeAttribute);
   if (hr == S_OK)
   {
      hr = pXmlNodeAttribute->get_text(&strName);
      *szValue = (LPTSTR)strName;

      _SafeCOMRelease(pXmlNodeAttribute);
   }
   else
      *szValue = NULL;

   return TRUE;
}

BOOL
pReadAttributeInterger (
   _In_ IXMLDOMNamedNodeMap *pXmlAttributeMap,
   _In_z_ LPWSTR szAttributeName,
   _Out_ PDWORD pdwValue
)
{
   HRESULT hr;
   int r;
   IXMLDOMNode *pXmlNodeAttribute = NULL;
   BSTR strName;
   DWORD dwValue;

   hr = pXmlAttributeMap->getNamedItem((BSTR)szAttributeName, &pXmlNodeAttribute);
   hr = pXmlNodeAttribute->get_text(&strName);

   r = swscanf_s(strName, L"%u", &dwValue);
   *pdwValue = dwValue;

   _SafeCOMRelease(pXmlNodeAttribute);

   return TRUE;
}

BOOL
pAddClassAttributes (
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPWSTR szClassName,
   _In_z_ PCLASS_CONFIG pClass,
   _In_ DWORD dwAttributesCount,
   _In_ PATTRIBUTE_CONFIG pAttributes
)
{
   HRESULT hr;
   WCHAR szXPath[MAX_PATH];
   IXMLDOMNodeList *pXMLNodeList = NULL;

   DWORD dwInitialLength;
   long lLength;

   BOOL bAttributeNotFound = FALSE;

   swprintf_s(szXPath, MAX_PATH, L"/configORADAD/schema/classes/class[@name=\"%s\"]/attribute", szClassName);

   hr = pXMLDoc->selectNodes(szXPath, &pXMLNodeList);
   hr = pXMLNodeList->get_length(&lLength);
   if (lLength == 0)
   {
      return TRUE;
   }

   dwInitialLength = pClass->dwAttributesCount;
   pClass->dwAttributesCount += lLength;

   if (dwInitialLength == 0)
   {
      pClass->pAttributes = (PATTRIBUTE_CONFIG*)_HeapAlloc(pClass->dwAttributesCount * sizeof(ATTRIBUTE_CONFIG));
   }
   else
   {
      pClass->pAttributes = (PATTRIBUTE_CONFIG*)HeapReAlloc(g_hHeap, HEAP_ZERO_MEMORY, pClass->pAttributes, pClass->dwAttributesCount * sizeof(ATTRIBUTE_CONFIG));
   }

   for (long i = 0; i < lLength; i++)
   {
      LPWSTR szAttributeName;
      PATTRIBUTE_CONFIG pAttribute;

      IXMLDOMNode *pXmlNode = NULL;
      IXMLDOMNamedNodeMap *pXmlClassMap = NULL;

      hr = pXMLNodeList->get_item(i, &pXmlNode);
      hr = pXmlNode->get_attributes(&pXmlClassMap);

      pReadAttributeString(pXmlClassMap, (LPWSTR)L"name", &szAttributeName);

      pAttribute = pFindAttribute(dwAttributesCount, pAttributes, szAttributeName);
      if (pAttribute == NULL)
      {
         Log(
            __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
            "Attribute not found (%S).", szAttributeName
         );
         bAttributeNotFound = TRUE;
      }

      pClass->pAttributes[dwInitialLength + i] = pAttribute;

      _SafeCOMRelease(pXmlClassMap);
      _SafeCOMRelease(pXmlNode);
   }

   _SafeCOMRelease(pXMLNodeList);

   if (bAttributeNotFound == TRUE)
      return FALSE;
   else
      return TRUE;
}

_Outptr_result_maybenull_
PATTRIBUTE_CONFIG
pFindAttribute (
   _In_ DWORD dwAttributesCount,
   _In_ PATTRIBUTE_CONFIG pAttributes,
   _In_z_ LPWSTR szName
)
{
   for (DWORD i = 0; i < dwAttributesCount; i++)
   {
      if (_wcsicmp(pAttributes[i].szName, szName) == 0)
         return &pAttributes[i];
   }

   return NULL;
}

BOOL
pGetAttributeByNameForRequest (
   _In_z_ LPWSTR szAttributeName,
   _Outptr_ PATTRIBUTE_CONFIG *pAttributes,
   _In_ PGLOBAL_CONFIG pGlobalConfig
)
{
   for (DWORD i = 0; i < pGlobalConfig->dwAttributesCount; i++)
   {
      if (_wcsicmp(szAttributeName, pGlobalConfig->pAttributes[i].szName) == 0)
      {
         *pAttributes = &pGlobalConfig->pAttributes[i];
         return TRUE;
      }
   }

   Log(
      __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
      "Attribute '%S' not found.", szAttributeName
   );
   return FALSE;
}

BOOL
pAddClassToRequest (
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPWSTR szClassName,
   PREQUEST_CONFIG pRequest,
   _In_ PGLOBAL_CONFIG pGlobalConfig
)
{
   HRESULT hr;
   BOOL bReturn = TRUE;

   WCHAR szXPath[MAX_PATH];
   long lLength;

   IXMLDOMNodeList *pXMLNodeListClass = NULL;
   IXMLDOMNodeList *pXMLNodeListAttributes = NULL;
   IXMLDOMNode *pXmlNode = NULL;
   IXMLDOMNamedNodeMap *pXmlClassMap = NULL;

   DWORD dwAttributesCount = 0;
   DWORD dwNewAttributes = 0;
   LPWSTR *szAttributes;
   LPWSTR szSubClasses;

   PATTRIBUTE_CONFIG *pNewAttributes;

   //
   // Find class
   //
   swprintf_s(szXPath, MAX_PATH, L"/configORADAD/schema/classes/class[@name=\"%s\"]", szClassName);

   hr = pXMLDoc->selectNodes(szXPath, &pXMLNodeListClass);
   hr = pXMLNodeListClass->get_length(&lLength);
   if (lLength != 1)
   {
      Log(
         __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_CRITICAL,
         "Class '%S' not found for request '%S'.", szClassName, pRequest->szName
      );
      return FALSE;
   }

   hr = pXMLNodeListClass->get_item(0, &pXmlNode);
   hr = pXmlNode->get_attributes(&pXmlClassMap);

   pReadAttributeString(pXmlClassMap, (LPWSTR)L"auxiliaryClass", &szSubClasses);
   if (szSubClasses != NULL)
      pAddClassesToRequest(pXMLDoc, szSubClasses, pRequest, pGlobalConfig);
   pReadAttributeString(pXmlClassMap, (LPWSTR)L"systemAuxiliaryClass", &szSubClasses);
   if (szSubClasses != NULL)
      pAddClassesToRequest(pXMLDoc, szSubClasses, pRequest, pGlobalConfig);

   hr = pXmlNode->get_childNodes(&pXMLNodeListAttributes);
   pXMLNodeListAttributes->get_length(&lLength);

   szAttributes = (LPWSTR*)_HeapAlloc(lLength * sizeof(LPWSTR));

   for (long i = 0; i < lLength; i++)
   {
      BSTR AttributeName;
      IXMLDOMNode *pXmlSubNode = NULL;

      hr = pXMLNodeListAttributes->get_item(i, &pXmlSubNode);
      hr = pXmlSubNode->get_nodeName(&AttributeName);

      if (_wcsicmp(AttributeName, L"attribute") == 0)
      {
         IXMLDOMNamedNodeMap *pXmlNodeAttributeAttributes = NULL;
         IXMLDOMNode *pXmlNodeAttributeName = NULL;
         BSTR szName;

         hr = pXmlSubNode->get_attributes(&pXmlNodeAttributeAttributes);
         hr = pXmlNodeAttributeAttributes->getNamedItem((BSTR)L"name", &pXmlNodeAttributeName);
         hr = pXmlNodeAttributeName->get_text(&szName);
         szAttributes[i] = (LPWSTR)szName;
         dwAttributesCount++;

         _SafeCOMRelease(pXmlNodeAttributeName);
         _SafeCOMRelease(pXmlNodeAttributeAttributes);
      }

      _SafeCOMRelease(pXmlSubNode);
   }

   _SafeCOMRelease(pXmlClassMap);
   _SafeCOMRelease(pXmlNode);
   _SafeCOMRelease(pXMLNodeListAttributes);
   _SafeCOMRelease(pXMLNodeListClass);

   //
   // First pass: count new attributes
   //
   DWORD dwClassNewAttributes;

   dwClassNewAttributes = dwAttributesCount;

   for (DWORD j = 0; j < dwAttributesCount; j++)
   {
      for (DWORD k = 0; k < pRequest->dwAttributesCount; k++)
      {
         if (_wcsicmp(szAttributes[j], (*pRequest->pAttributes[k]).szName) == 0)
            dwClassNewAttributes--;
      }
   }

   dwNewAttributes += dwClassNewAttributes;

   //
   // Second pass: add new attributes
   //
   if (dwNewAttributes > 0)
   {
      if (pRequest->pAttributes == NULL)
      {
         pNewAttributes = (PATTRIBUTE_CONFIG*)HeapAlloc(
            g_hHeap,
            HEAP_ZERO_MEMORY,
            dwNewAttributes * sizeof(PATTRIBUTE_CONFIG)
         );
      }
      else
      {
         pNewAttributes = (PATTRIBUTE_CONFIG*)HeapReAlloc(
            g_hHeap,
            HEAP_ZERO_MEMORY,
            pRequest->pAttributes,
            (pRequest->dwAttributesCount + dwNewAttributes) * sizeof(PATTRIBUTE_CONFIG)
         );
      }
      pRequest->pAttributes = pNewAttributes;

      for (DWORD j = 0; j < dwAttributesCount; j++)
      {
         BOOL bAddAttribute = TRUE;

         for (DWORD k = 0; k < pRequest->dwAttributesCount; k++)
         {
            if (_wcsicmp(szAttributes[j], (*pRequest->pAttributes[k]).szName) == 0)
            {
               bAddAttribute = FALSE;
            }
         }

         if (bAddAttribute == TRUE)
         {
            BOOL bResult;

            bResult = pGetAttributeByNameForRequest(szAttributes[j], &pRequest->pAttributes[pRequest->dwAttributesCount], pGlobalConfig);
            if (bResult == FALSE)
               bReturn = FALSE;
            else
               pRequest->dwAttributesCount++;
         }
      }
   }

   return bReturn;
}

BOOL
pAddClassesToRequest (
   _In_ IXMLDOMDocument2 *pXMLDoc,
   _In_z_ LPWSTR szClassName,
   PREQUEST_CONFIG pRequest,
   _In_ PGLOBAL_CONFIG pGlobalConfig
)
{
   LPWSTR szToken;
   LPWSTR szTokenContext = NULL;

   if (szClassName == NULL)
      return TRUE;

   szToken = wcstok_s(szClassName, L",", &szTokenContext);
   while (szToken != NULL)
   {
      BOOL bResult;

      bResult = pAddClassToRequest(pXMLDoc, szToken, pRequest, pGlobalConfig);
      if (bResult == FALSE)
         return FALSE;

      szToken = wcstok_s(NULL, L",", &szTokenContext);
   }

   return TRUE;
}

BOOL
pXmlParseRequest (
   _In_ IXMLDOMDocument2 *pXMLDoc,
   IXMLDOMNode *pXmlNodeRequet,
   PREQUEST_CONFIG pRequest,
   _In_ PGLOBAL_CONFIG pGlobalConfig
)
{
   HRESULT hr;
   IXMLDOMNodeList *pXmlNodeListRequet = NULL;

   long lLength;

   hr = pXmlNodeRequet->get_childNodes(&pXmlNodeListRequet);

   hr = pXmlNodeListRequet->get_length(&lLength);

   for (long i = 0; i < lLength; i++)
   {
      IXMLDOMNode *pXmlNode = NULL;

      BSTR strNodeName;
      BSTR strNodeText;

      hr = pXmlNodeListRequet->get_item(i, &pXmlNode);
      hr = pXmlNode->get_nodeName(&strNodeName);
      hr = pXmlNode->get_text(&strNodeText);

      if ((_wcsicmp(strNodeName, L"name") == 0) && (wcslen(strNodeText) > 0))
         pRequest->szName = strNodeText;
      else if ((_wcsicmp(strNodeName, L"filter") == 0) && (wcslen(strNodeText) > 0))
         pRequest->szFilter = strNodeText;
      else if ((_wcsicmp(strNodeName, L"scope") == 0) && (wcslen(strNodeText) > 0))
      {
         if (_wcsicmp(strNodeText, L"base") == 0)
            pRequest->dwScope = 0;        // LDAP_SCOPE_BASE;
         else if (_wcsicmp(strNodeText, L"onelevel") == 0)
            pRequest->dwScope = 1;        // LDAP_SCOPE_ONELEVEL;
         else if (_wcsicmp(strNodeText, L"subtree") == 0)
            pRequest->dwScope = 2;        // LDAP_SCOPE_SUBTREE;
      }
      else if ((_wcsicmp(strNodeName, L"base") == 0) && (wcslen(strNodeText) > 0))
      {
         LPWSTR szToken;
         LPWSTR szTokenContext = NULL;

         szToken = wcstok_s(strNodeText, L",", &szTokenContext);
         while (szToken != NULL)
         {
            if (_wcsicmp(szToken, STR_ROOTDSE) == 0)
               pRequest->dwBase |= BASE_ROOTDSE;
            else if (_wcsicmp(szToken, STR_DOMAIN) == 0)
               pRequest->dwBase |= BASE_DOMAIN;
            else if (_wcsicmp(szToken, STR_CONFIGURATION) == 0)
               pRequest->dwBase |= BASE_CONFIGURATION;
            else if (_wcsicmp(szToken, STR_SCHEMA) == 0)
               pRequest->dwBase |= BASE_SCHEMA;
            else if (_wcsicmp(szToken, STR_DOMAIN_DNS) == 0)
               pRequest->dwBase |= BASE_DOMAIN_DNS;
            else if (_wcsicmp(szToken, STR_FOREST_DNS) == 0)
               pRequest->dwBase |= BASE_FOREST_DNS;

            szToken = wcstok_s(NULL, L",", &szTokenContext);
         }
      }
      else if ((_wcsicmp(strNodeName, L"classes") == 0) && (wcslen(strNodeText) > 0))
      {
         BOOL bResult;

         bResult = pAddClassesToRequest(pXMLDoc, strNodeText, pRequest, pGlobalConfig);

         if (bResult == FALSE)
            return FALSE;
      }

      _SafeCOMRelease(pXmlNode);
   }

   _SafeCOMRelease(pXmlNodeListRequet);

   return TRUE;
}

DWORD
pReadUInteger (
   _In_opt_z_ LPCWSTR szValue
)
{
   DWORD dwResult;

   if (szValue == NULL)
      return 0;

   if (wcslen(szValue)==0)
      return 0;

   if (swscanf_s(szValue, L"%u", &dwResult) == 1)
      return dwResult;
   else
      return 0;
}

BOOL
pReadBoolean (
   _In_opt_z_ LPCWSTR szValue
)
{
   if (szValue == NULL)
      return FALSE;

   if (_wcsicmp(szValue, L"true") == 0)
      return TRUE;
   else if (wcscmp(szValue, L"1") == 0)
      return TRUE;
   else
      return FALSE;
}