//******************************************************************
//
// Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <string.h>
#include "ocstack.h"
#include "ocstackconfig.h"
#include "ocstackinternal.h"
#include "ocobserve.h"
#include "ocresourcehandler.h"
#include "ocrandom.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocserverrequest.h"
#include "logger.h"

#include <coap/utlist.h>
#include <coap/pdu.h>
#include <coap/coap.h>

// Module Name
#define MOD_NAME "ocobserve"

#define TAG  "OIC_RI_OBSERVE"

#define VERIFY_NON_NULL(arg) { if (!arg) {OIC_LOG(FATAL, TAG, #arg " is NULL"); goto exit;} }

static struct ResourceObserver * g_serverObsList = NULL;
/**
 * Determine observe QOS based on the QOS of the request.
 * The qos passed as a parameter overrides what the client requested.
 * If we want the client preference taking high priority make:
 *     qos = resourceObserver->qos;
 *
 * @param method RESTful method.
 * @param resourceObserver Observer.
 * @param appQoS Quality of service.
 * @return The quality of service of the observer.
 */
static OCQualityOfService DetermineObserverQoS(OCMethod method,
        ResourceObserver * resourceObserver, OCQualityOfService appQoS)
{
    if (!resourceObserver)
    {
        OIC_LOG(ERROR, TAG, "DetermineObserverQoS called with invalid resourceObserver");
        return OC_NA_QOS;
    }

    OCQualityOfService decidedQoS = appQoS;
    if (appQoS == OC_NA_QOS)
    {
        decidedQoS = resourceObserver->qos;
    }

    if (appQoS != OC_HIGH_QOS)
    {
        OIC_LOG_V(INFO, TAG, "Current NON count for this observer is %d",
                resourceObserver->lowQosCount);
#ifdef WITH_PRESENCE
        if ((resourceObserver->forceHighQos \
                || resourceObserver->lowQosCount >= MAX_OBSERVER_NON_COUNT) \
                && method != OC_REST_PRESENCE)
#else
        if (resourceObserver->forceHighQos \
                || resourceObserver->lowQosCount >= MAX_OBSERVER_NON_COUNT)
#endif
        {
            resourceObserver->lowQosCount = 0;
            // at some point we have to to send CON to check on the
            // availability of observer
            OIC_LOG(INFO, TAG, "This time we are sending the  notification as High qos");
            decidedQoS = OC_HIGH_QOS;
        }
        else
        {
            (resourceObserver->lowQosCount)++;
        }
    }
    return decidedQoS;
}

/**
 * Create a get request and pass to entityhandler to notify specific observer.
 *
 * @param observer Observer that need to be notified.
 * @param qos Quality of service of resource.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
static OCStackResult SendObserveNotification(ResourceObserver *observer,
                                             OCQualityOfService qos)
{
    OCStackResult result = OC_STACK_ERROR;
    OCServerRequest * request = NULL;

    result = AddServerRequest(&request, 0, 0, 1, OC_REST_GET,
                              0, observer->resource->sequenceNum, qos,
                              observer->query, NULL, OC_FORMAT_UNDEFINED, NULL,
                              observer->token, observer->tokenLength,
                              observer->resUri, 0, observer->acceptFormat,
                              observer->acceptVersion, &observer->devAddr);

    if (request)
    {
        request->observeResult = OC_STACK_OK;
        if (result == OC_STACK_OK)
        {
            ResourceHandling resHandling = OC_RESOURCE_VIRTUAL;
            OCResource *resource = NULL;
            result = DetermineResourceHandling (request, &resHandling, &resource);
            if (result == OC_STACK_OK)
            {
                result = ProcessRequest(resHandling, resource, request);
                // Reset Observer TTL.
                observer->TTL = GetTicks(MAX_OBSERVER_TTL_SECONDS * MILLISECONDS_PER_SECOND);
            }
        }
#if defined(__TIZENRT__)
        FindAndDeleteServerRequest(request);
#endif
    }

    return result;
}

#ifdef WITH_PRESENCE
OCStackResult SendAllObserverNotification (OCMethod method, OCResource *resPtr, uint32_t maxAge,
        OCPresenceTrigger trigger, OCResourceType *resourceType, OCQualityOfService qos)
#else
OCStackResult SendAllObserverNotification (OCMethod method, OCResource *resPtr, uint32_t maxAge,
        OCQualityOfService qos)
#endif
{
    OIC_LOG(INFO, TAG, "Entering SendObserverNotification");
    if (!resPtr)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCStackResult result = OC_STACK_ERROR;
    ResourceObserver * resourceObserver = g_serverObsList;
    uint8_t numObs = 0;
    OCServerRequest * request = NULL;
    bool observeErrorFlag = false;

    // Find clients that are observing this resource
    while (resourceObserver)
    {
        if (resourceObserver->resource == resPtr)
        {
            numObs++;
#ifdef WITH_PRESENCE
            if (method != OC_REST_PRESENCE)
            {
#endif
                qos = DetermineObserverQoS(method, resourceObserver, qos);
                result = SendObserveNotification(resourceObserver, qos);
#ifdef WITH_PRESENCE
            }
            else
            {
                OCEntityHandlerResponse ehResponse = {0};

                //This is effectively the implementation for the presence entity handler.
                OIC_LOG(DEBUG, TAG, "This notification is for Presence");
                result = AddServerRequest(&request, 0, 0, 1, OC_REST_GET,
                        0, resPtr->sequenceNum, qos, resourceObserver->query,
                        NULL, OC_FORMAT_UNDEFINED, NULL,
                        resourceObserver->token, resourceObserver->tokenLength,
                        resourceObserver->resUri, 0, resourceObserver->acceptFormat,
                        resourceObserver->acceptVersion, &resourceObserver->devAddr);

                if (result == OC_STACK_OK)
                {
                    OCPresencePayload* presenceResBuf = OCPresencePayloadCreate(
                            resPtr->sequenceNum, maxAge, trigger,
                            resourceType ? resourceType->resourcetypename : NULL);

                    if (!presenceResBuf)
                    {
#if defined(__TIZENRT__)
                        FindAndDeleteServerRequest(request);
#endif
                        return OC_STACK_NO_MEMORY;
                    }

                    if (result == OC_STACK_OK)
                    {
                        ehResponse.ehResult = OC_EH_OK;
                        ehResponse.payload = (OCPayload*)presenceResBuf;
                        ehResponse.persistentBufferFlag = 0;
                        ehResponse.requestHandle = (OCRequestHandle) request;
                        ehResponse.resourceHandle = (OCResourceHandle) resPtr;
                        OICStrcpy(ehResponse.resourceUri, sizeof(ehResponse.resourceUri),
                                resourceObserver->resUri);
                        result = OCDoResponse(&ehResponse);
                    }

                    OCPresencePayloadDestroy(presenceResBuf);
#if defined(__TIZENRT__)
                    FindAndDeleteServerRequest(request);
#endif
                }
            }
#endif

            // Since we are in a loop, set an error flag to indicate at least one error occurred.
            if (result != OC_STACK_OK)
            {
                observeErrorFlag = true;
            }
        }
        resourceObserver = resourceObserver->next;
    }

    if (numObs == 0)
    {
        OIC_LOG(INFO, TAG, "Resource has no observers");
        result = OC_STACK_NO_OBSERVERS;
    }
    else if (observeErrorFlag)
    {
        OIC_LOG(ERROR, TAG, "Observer notification error");
        result = OC_STACK_ERROR;
    }
    return result;
}

OCStackResult SendListObserverNotification (OCResource * resource,
        OCObservationId  *obsIdList, uint8_t numberOfIds,
        const OCRepPayload *payload,
        uint32_t maxAge,
        OCQualityOfService qos)
{
    (void)maxAge;
    if (!resource || !obsIdList || !payload)
    {
        return OC_STACK_INVALID_PARAM;
    }

    uint8_t numIds = numberOfIds;
    ResourceObserver *observer = NULL;
    uint8_t numSentNotification = 0;
    OCServerRequest * request = NULL;
    OCStackResult result = OC_STACK_ERROR;
    bool observeErrorFlag = false;

    OIC_LOG(INFO, TAG, "Entering SendListObserverNotification");
    while(numIds)
    {
        observer = GetObserverUsingId (*obsIdList);
        if (observer)
        {
            // Found observer - verify if it matches the resource handle
            if (observer->resource == resource)
            {
                qos = DetermineObserverQoS(OC_REST_GET, observer, qos);


                result = AddServerRequest(&request, 0, 0, 1, OC_REST_GET,
                        0, resource->sequenceNum, qos, observer->query,
                        NULL, OC_FORMAT_UNDEFINED, NULL, observer->token, observer->tokenLength,
                        observer->resUri, 0, observer->acceptFormat,
                        observer->acceptVersion, &observer->devAddr);

                if (request)
                {
                    request->observeResult = OC_STACK_OK;
                    if (result == OC_STACK_OK)
                    {
                        OCEntityHandlerResponse ehResponse = {0};
                        ehResponse.ehResult = OC_EH_OK;
                        ehResponse.payload = (OCPayload*)OCRepPayloadCreate();
                        if (!ehResponse.payload)
                        {
                            FindAndDeleteServerRequest(request);
                            continue;
                        }
                        memcpy(ehResponse.payload, payload, sizeof(*payload));
                        ehResponse.persistentBufferFlag = 0;
                        ehResponse.requestHandle = (OCRequestHandle) request;
                        ehResponse.resourceHandle = (OCResourceHandle) resource;
                        result = OCDoResponse(&ehResponse);
                        if (result == OC_STACK_OK)
                        {
                            OIC_LOG_V(INFO, TAG, "Observer id %d notified.", *obsIdList);

                            // Increment only if OCDoResponse is successful
                            numSentNotification++;

                            OICFree(ehResponse.payload);
                        }
                        else
                        {
                            OIC_LOG_V(INFO, TAG, "Error notifying observer id %d.", *obsIdList);
                        }
                        // Reset Observer TTL.
                        observer->TTL =
                                GetTicks(MAX_OBSERVER_TTL_SECONDS * MILLISECONDS_PER_SECOND);
                    }
                    else
                    {
                        FindAndDeleteServerRequest(request);
                    }
                }
                // Since we are in a loop, set an error flag to indicate
                // at least one error occurred.
                if (result != OC_STACK_OK)
                {
                    observeErrorFlag = true;
                }
            }
        }
        obsIdList++;
        numIds--;
    }

    if (numSentNotification == numberOfIds && !observeErrorFlag)
    {
        return OC_STACK_OK;
    }
    else if (numSentNotification == 0)
    {
        return OC_STACK_NO_OBSERVERS;
    }
    else
    {
        OIC_LOG(ERROR, TAG, "Observer notification error");
        return OC_STACK_ERROR;
    }
}

OCStackResult GenerateObserverId (OCObservationId *observationId)
{
    ResourceObserver *resObs = NULL;

    OIC_LOG(INFO, TAG, "Entering GenerateObserverId");
    VERIFY_NON_NULL (observationId);

    do
    {
        if (!OCGetRandomBytes((uint8_t*)observationId, sizeof(OCObservationId)))
        {
            OIC_LOG(ERROR, TAG, "Failed to generate random observationId");
            goto exit;
        }

        // Check if observation Id already exists
        resObs = GetObserverUsingId (*observationId);
    } while (NULL != resObs);

    OIC_LOG_V(INFO, TAG, "GeneratedObservation ID is %u", *observationId);

    return OC_STACK_OK;
exit:
    return OC_STACK_ERROR;
}

OCStackResult AddObserver (const char         *resUri,
                           const char         *query,
                           OCObservationId    obsId,
                           CAToken_t          token,
                           uint8_t            tokenLength,
                           OCResource         *resHandle,
                           OCQualityOfService qos,
                           OCPayloadFormat    acceptFormat,
                           uint16_t           acceptVersion,
                           const OCDevAddr    *devAddr)
{
    // Check if resource exists and is observable.
    if (!resHandle)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (!(resHandle->resourceProperties & OC_OBSERVABLE))
    {
        return OC_STACK_RESOURCE_ERROR;
    }

    if (!resUri || !token)
    {
        return OC_STACK_INVALID_PARAM;
    }

    ResourceObserver *obsNode = (ResourceObserver *) OICCalloc(1, sizeof(ResourceObserver));
    if (obsNode)
    {
        obsNode->observeId = obsId;

        obsNode->resUri = OICStrdup(resUri);
        VERIFY_NON_NULL (obsNode->resUri);

        obsNode->qos = qos;
        obsNode->acceptFormat = acceptFormat;
        obsNode->acceptVersion = acceptVersion;
        if (query)
        {
            obsNode->query = OICStrdup(query);
            VERIFY_NON_NULL (obsNode->query);
        }
        // If tokenLength is zero, the return value depends on the
        // particular library implementation (it may or may not be a null pointer).
        if (tokenLength)
        {
            obsNode->token = (CAToken_t)OICMalloc(tokenLength);
            VERIFY_NON_NULL (obsNode->token);
            memcpy(obsNode->token, token, tokenLength);
        }
        obsNode->tokenLength = tokenLength;

        obsNode->devAddr = *devAddr;
        obsNode->resource = resHandle;

        if ((strcmp(resUri, OC_RSRVD_PRESENCE_URI) == 0))
        {
            obsNode->TTL = 0;
        }
        else
        {
            obsNode->TTL = GetTicks(MAX_OBSERVER_TTL_SECONDS * MILLISECONDS_PER_SECOND);
        }

        LL_APPEND (g_serverObsList, obsNode);

        return OC_STACK_OK;
    }

exit:
    if (obsNode)
    {
        OICFree(obsNode->resUri);
        OICFree(obsNode->query);
        OICFree(obsNode);
    }
    return OC_STACK_NO_MEMORY;
}

/*
 * This function checks if the node is past its time to live and
 * deletes it if timed-out. Calling this function with a  presence callback
 * with ttl set to 0 will not delete anything as presence nodes have
 * their own mechanisms for timeouts. A null argument will cause the function to
 * silently return.
 */
static void CheckTimedOutObserver(ResourceObserver* observer)
{
    if (!observer || observer->TTL == 0)
    {
        return;
    }

    coap_tick_t now = 0;
    coap_ticks(&now);

    if (observer->TTL < now)
    {
        // Send confirmable notification message to observer.
        OIC_LOG(INFO, TAG, "Sending High-QoS notification to observer");
        SendObserveNotification(observer, OC_HIGH_QOS);
    }
}

ResourceObserver* GetObserverUsingId (const OCObservationId observeId)
{
    ResourceObserver *out = NULL;

    LL_FOREACH (g_serverObsList, out)
    {
        if (out->observeId == observeId)
        {
            return out;
        }
        CheckTimedOutObserver(out);
    }

    if (!out)
    {
        OIC_LOG(INFO, TAG, "Observer node not found!!");
    }
    return NULL;
}

ResourceObserver* GetObserverUsingToken (const CAToken_t token, uint8_t tokenLength)
{
    if (token)
    {
        OIC_LOG(INFO, TAG, "Looking for token");
        OIC_LOG_BUFFER(INFO, TAG, (const uint8_t *)token, tokenLength);

        ResourceObserver *out = NULL;
        LL_FOREACH (g_serverObsList, out)
        {
            /* de-annotate below line if want to see all token in cbList */
            //OIC_LOG_BUFFER(INFO, TAG, (const uint8_t *)out->token, tokenLength);
            if ((memcmp(out->token, token, tokenLength) == 0))
            {
                OIC_LOG(INFO, TAG, "Found in observer list");
                return out;
            }
            CheckTimedOutObserver(out);
        }
    }
    else
    {
        OIC_LOG(ERROR, TAG, "Passed in NULL token");
    }

    OIC_LOG(INFO, TAG, "Observer node not found!!");
    return NULL;
}

OCStackResult DeleteObserverUsingToken (CAToken_t token, uint8_t tokenLength)
{
    if (!token)
    {
        return OC_STACK_INVALID_PARAM;
    }

    ResourceObserver *obsNode = GetObserverUsingToken (token, tokenLength);
    if (obsNode)
    {
        OIC_LOG_V(INFO, TAG, "deleting observer id  %u with token", obsNode->observeId);
        OIC_LOG_BUFFER(INFO, TAG, (const uint8_t *)obsNode->token, tokenLength);
        LL_DELETE (g_serverObsList, obsNode);
        OICFree(obsNode->resUri);
        OICFree(obsNode->query);
        OICFree(obsNode->token);
        OICFree(obsNode);
    }
    // it is ok if we did not find the observer...
    return OC_STACK_OK;
}

OCStackResult DeleteObserverUsingDevAddr(const OCDevAddr *devAddr)
{
    if (!devAddr)
    {
        return OC_STACK_INVALID_PARAM;
    }

    ResourceObserver *out = NULL;
    ResourceObserver *tmp = NULL;
    LL_FOREACH_SAFE(g_serverObsList, out, tmp)
    {
        if (out)
        {
            if ((strcmp(out->devAddr.addr, devAddr->addr) == 0)
                    && out->devAddr.port == devAddr->port)
            {
                OIC_LOG_V(INFO, TAG, "deleting observer id  %u with %s:%u",
                          out->observeId, out->devAddr.addr, out->devAddr.port);
                OCStackFeedBack(out->token, out->tokenLength, OC_OBSERVER_NOT_INTERESTED);
            }
        }
    }

    return OC_STACK_OK;
}

void DeleteObserverList()
{
    ResourceObserver *out = NULL;
    ResourceObserver *tmp = NULL;
    LL_FOREACH_SAFE (g_serverObsList, out, tmp)
    {
        if (out)
        {
            DeleteObserverUsingToken ((out->token), out->tokenLength);
        }
    }
    g_serverObsList = NULL;
}

/*
 * CA layer expects observe registration/de-reg/notiifcations to be passed as a header
 * option, which breaks the protocol abstraction requirement between RI & CA, and
 * has to be fixed in the future. The function below adds the header option for observe.
 * It should be noted that the observe header option is assumed to be the first option
 * in the list of user defined header options and hence it is inserted at the front
 * of the header options list and number of options adjusted accordingly.
 */
OCStackResult
CreateObserveHeaderOption (CAHeaderOption_t **caHdrOpt,
                           OCHeaderOption *ocHdrOpt,
                           uint8_t numOptions,
                           uint8_t observeFlag)
{
    if (!caHdrOpt)
    {
        return OC_STACK_INVALID_PARAM;
    }

    if (numOptions > 0 && !ocHdrOpt)
    {
        OIC_LOG (INFO, TAG, "options are NULL though number is non zero");
        return OC_STACK_INVALID_PARAM;
    }

    CAHeaderOption_t *tmpHdrOpt = NULL;

    tmpHdrOpt = (CAHeaderOption_t *) OICCalloc ((numOptions+1), sizeof(CAHeaderOption_t));
    if (NULL == tmpHdrOpt)
    {
        return OC_STACK_NO_MEMORY;
    }
    tmpHdrOpt[0].protocolID = CA_COAP_ID;
    tmpHdrOpt[0].optionID = COAP_OPTION_OBSERVE;
    tmpHdrOpt[0].optionLength = sizeof(uint8_t);
    tmpHdrOpt[0].optionData[0] = observeFlag;
    for (uint8_t i = 0; i < numOptions; i++)
    {
        memcpy (&(tmpHdrOpt[i+1]), &(ocHdrOpt[i]), sizeof(CAHeaderOption_t));
    }

    *caHdrOpt = tmpHdrOpt;
    return OC_STACK_OK;
}

/*
 * CA layer passes observe information to the RI layer as a header option, which
 * breaks the protocol abstraction requirement between RI & CA, and has to be fixed
 * in the future. The function below removes the observe header option and processes it.
 * It should be noted that the observe header option is always assumed to be the first
 * option in the list of user defined header options and hence it is deleted from the
 * front of the header options list and the number of options is adjusted accordingly.
 */
OCStackResult
GetObserveHeaderOption (uint32_t * observationOption,
                        CAHeaderOption_t *options,
                        uint8_t * numOptions)
{
    if (!observationOption)
    {
        return OC_STACK_INVALID_PARAM;
    }

    if (!options || !numOptions)
    {
        OIC_LOG (INFO, TAG, "No options present");
        return OC_STACK_OK;
    }

    for(uint8_t i = 0; i < *numOptions; i++)
    {
        if (options[i].protocolID == CA_COAP_ID &&
                options[i].optionID == COAP_OPTION_OBSERVE)
        {
            *observationOption = options[i].optionData[0];
            for(uint8_t c = i; c < *numOptions-1; c++)
            {
                options[i] = options[i+1];
            }
            (*numOptions)--;
            return OC_STACK_OK;
        }
    }
    return OC_STACK_OK;
}

OCStackResult
HandleVirtualObserveRequest(OCServerRequest *request)
{
    OCStackResult result = OC_STACK_OK;
    if (request->notificationFlag)
    {
        // The request is to send an observe payload, not register/deregister an observer
        goto exit;
    }
    OCVirtualResources virtualUriInRequest = OC_UNKNOWN_URI;
    virtualUriInRequest = GetTypeOfVirtualURI(request->resourceUrl);
    if (virtualUriInRequest != OC_WELL_KNOWN_URI)
    {
        // OC_WELL_KNOWN_URI is currently the only virtual resource that may be observed
        goto exit;
    }
    OCResource *resourcePtr = NULL;
    resourcePtr = FindResourceByUri(OC_RSRVD_WELL_KNOWN_URI);
    if (NULL == resourcePtr)
    {
        OIC_LOG(FATAL, TAG, "Well-known URI not found.");
        result = OC_STACK_ERROR;
        goto exit;
    }
    if (request->observationOption == OC_OBSERVE_REGISTER)
    {
        OIC_LOG(INFO, TAG, "Observation registration requested");
        ResourceObserver *obs = GetObserverUsingToken (request->requestToken,
                                                       request->tokenLength);
        if (obs)
        {
            OIC_LOG (INFO, TAG, "Observer with this token already present");
            OIC_LOG (INFO, TAG, "Possibly re-transmitted CON OBS request");
            OIC_LOG (INFO, TAG, "Not adding observer. Not responding to client");
            OIC_LOG (INFO, TAG, "The first request for this token is already ACKED.");
            result = OC_STACK_DUPLICATE_REQUEST;
            goto exit;
        }
        OCObservationId obsId;
        result = GenerateObserverId(&obsId);
        if (result == OC_STACK_OK)
        {
            result = AddObserver ((const char*)(request->resourceUrl),
                                  (const char *)(request->query),
                                  obsId, request->requestToken, request->tokenLength,
                                  resourcePtr, request->qos, request->acceptFormat,
                                  request->acceptVersion, &request->devAddr);
        }
        if (result == OC_STACK_OK)
        {
            OIC_LOG(INFO, TAG, "Added observer successfully");
            request->observeResult = OC_STACK_OK;
        }
        else if (result == OC_STACK_RESOURCE_ERROR)
        {
            OIC_LOG(INFO, TAG, "The Resource is not active, discoverable or observable");
            request->observeResult = OC_STACK_ERROR;
        }
        else
        {
            // The error in observeResult for the request will be used when responding to this
            // request by omitting the observation option/sequence number.
            request->observeResult = OC_STACK_ERROR;
            OIC_LOG(ERROR, TAG, "Observer Addition failed");
        }
    }
    else if (request->observationOption == OC_OBSERVE_DEREGISTER)
    {
        OIC_LOG(INFO, TAG, "Deregistering observation requested");
        result = DeleteObserverUsingToken (request->requestToken, request->tokenLength);
        if (result == OC_STACK_OK)
        {
            OIC_LOG(INFO, TAG, "Removed observer successfully");
            request->observeResult = OC_STACK_OK;
            // There should be no observe option header for de-registration response.
            // Set as an invalid value here so we can detect it later and remove the field in response.
            request->observationOption = MAX_SEQUENCE_NUMBER + 1;
        }
        else
        {
            request->observeResult = OC_STACK_ERROR;
            OIC_LOG(ERROR, TAG, "Observer Removal failed");
        }
    }
    // Whether the observe request succeeded or failed, the request is processed as normal
    // and excludes/includes the OBSERVE option depending on request->observeResult
    result = OC_STACK_OK;

exit:
    return result;
}
