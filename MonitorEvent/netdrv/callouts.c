#include "public.h"
#include "devctrl.h"
#include "datalinkctx.h"
#include "establishedctx.h"
#include "tcpctx.h"
#include "callouts.h"

#include <ws2ipdef.h>
#include <in6addr.h>
#include <ip2string.h>

#define INITGUID
#include <guiddef.h>

static GUID		g_providerGuid;
static GUID		g_calloutGuid_flow_established_v4;
static GUID		g_calloutGuid_flow_established_v6;
static GUID		g_calloutGuid_ale_connectredirect_v4;
static GUID		g_calloutGuid_ale_connectredirect_v6;
static GUID		g_calloutGuid_inbound_mac_etherent;
static GUID		g_calloutGuid_outbound_mac_etherent;
static GUID		g_calloutGuid_inbound_mac_native;
static GUID		g_calloutGuid_outbound_mac_native;

static UINT32	g_calloutId_flow_established_v4;
static UINT32	g_calloutId_flow_established_v6;
static UINT32	g_calloutId_inbound_mac_etherent;
static UINT32	g_calloutId_outbound_mac_etherent;
static UINT32	g_calloutId_inbound_mac_native;
static UINT32	g_calloutId_outbound_mac_native;
static UINT32	g_calloutId_ale_connectredirect_v4;
static UINT32	g_calloutId_ale_connectredirect_v6;

static GUID		g_sublayerGuid;
static HANDLE	g_engineHandle = NULL;

// extern
BOOLEAN g_monitorflag = FALSE;
BOOLEAN g_redirectflag = FALSE;

static NPAGED_LOOKASIDE_LIST	g_callouts_flowCtxPacketsLAList;
static KSPIN_LOCK				g_callouts_flowspinlock;

static NPAGED_LOOKASIDE_LIST	g_callouts_datalinkPacktsList;
static KSPIN_LOCK				g_callouts_datalinkspinlock;

typedef enum _NF_DIRECTION
{
	NF_D_IN = 1,		// Incoming TCP connection or UDP packet
	NF_D_OUT = 2,		// Outgoing TCP connection or UDP packet
	NF_D_BOTH = 3		// Any direction
} NF_DIRECTION;

#define NFSDK_SUBLAYER_NAME L"Dark Sublayer"
#define NFSDK_RECV_SUBLAYER_NAME L"Dark Recv Sublayer"
#define NFSDK_PROVIDER_NAME L"Dark Provider"

// OutPut
void DbgPrintAddress(int ipFamily, void* addr, char* name, UINT64 id)
{
	if (ipFamily == AF_INET)
	{
		struct sockaddr_in* pAddr = (struct sockaddr_in*)addr;

		KdPrint((DPREFIX"DbgPrintAddress[%I64u] %s=%x:%d\n",
			id, name, pAddr->sin_addr.s_addr, htons(pAddr->sin_port)));
	}
	else
	{
		struct sockaddr_in6* pAddr = (struct sockaddr_in6*)addr;

		KdPrint((DPREFIX"DbgPrintAddress[%I64u] %s=[%x:%x:%x:%x:%x:%x:%x:%x]:%d\n",
			id, name,
			pAddr->sin6_addr.u.Word[0],
			pAddr->sin6_addr.u.Word[1],
			pAddr->sin6_addr.u.Word[2],
			pAddr->sin6_addr.u.Word[3],
			pAddr->sin6_addr.u.Word[4],
			pAddr->sin6_addr.u.Word[5],
			pAddr->sin6_addr.u.Word[6],
			pAddr->sin6_addr.u.Word[7],
			htons(pAddr->sin6_port)));
	}
}

// Callouts Established Callbacks
VOID helper_callout_classFn_flowEstablished(
	_In_ const FWPS_INCOMING_VALUES0* inFixedValues,
	_In_ const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	_Inout_opt_ void* layerData,
	_In_opt_ const void* classifyContext,
	_In_ const FWPS_FILTER3* filter,
	_In_ UINT64 flowContext,
	_Inout_ FWPS_CLASSIFY_OUT0* classifyOut
	)
{
	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(flowContext);
	UNREFERENCED_PARAMETER(classifyContext);

	NTSTATUS status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE lh;
	PNF_CALLOUT_FLOWESTABLISHED_INFO flowContextLocal = NULL;
	
	// �رռ�ص�ʱ�򣬲����κβ���
	if (g_monitorflag == FALSE)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
		{
			classifyOut->flags &= ~FWPS_RIGHT_ACTION_WRITE;
		}
		return;
	}

	sl_lock(&g_callouts_flowspinlock, &lh);
	flowContextLocal = (PNF_CALLOUT_FLOWESTABLISHED_INFO)ExAllocateFromNPagedLookasideList(&g_callouts_flowCtxPacketsLAList);
	sl_unlock(&lh);
	if (flowContextLocal == NULL)
	{
		status = STATUS_NO_MEMORY;
		goto Exit;
	}
	RtlSecureZeroMemory(flowContextLocal, sizeof(NF_CALLOUT_FLOWESTABLISHED_INFO));

	flowContextLocal->refCount = 1;
	// ����Э��
	flowContextLocal->addressFamily= 
		(inFixedValues->layerId == FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4) ? AF_INET : AF_INET6;

	flowContextLocal->flowId = inMetaValues->flowHandle;
	flowContextLocal->layerId = FWPS_LAYER_OUTBOUND_MAC_FRAME_ETHERNET;
	flowContextLocal->calloutId = &g_calloutId_outbound_mac_etherent;

	if (flowContextLocal->addressFamily == AF_INET)
	{
		flowContextLocal->ipv4LocalAddr =
			RtlUlongByteSwap(
				inFixedValues->incomingValue\
				[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_ADDRESS].value.uint32
			);
		flowContextLocal->ipv4toRemoteAddr =
			RtlUlongByteSwap(
				inFixedValues->incomingValue\
				[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_ADDRESS].value.uint32
			);
		flowContextLocal->protocol =
			inFixedValues->incomingValue\
			[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_PROTOCOL].value.uint8;
		flowContextLocal->toLocalPort =
			inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_PORT].value.uint16;
		flowContextLocal->toRemotePort =
			inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_PORT].value.uint16;
	}
	else
	{
		RtlCopyMemory(
			(UINT8*)&flowContextLocal->localAddr,
			inFixedValues->incomingValue\
			[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6_IP_LOCAL_ADDRESS].value.byteArray16->byteArray16,
			16
		);
		RtlCopyMemory(
			(UINT8*)&flowContextLocal->RemoteAddr,
			inFixedValues->incomingValue\
			[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6_IP_REMOTE_ADDRESS].value.byteArray16->byteArray16,
			16
		);
		flowContextLocal->protocol =
			inFixedValues->incomingValue\
			[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6_IP_PROTOCOL].value.uint8;
		flowContextLocal->toLocalPort =
			inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6_IP_LOCAL_PORT].value.uint16;
		flowContextLocal->toRemotePort =
			inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6_IP_REMOTE_PORT].value.uint16;
	}

	flowContextLocal->processId = inMetaValues->processId;
	flowContextLocal->processPathSize = inMetaValues->processPath->size;
	RtlCopyMemory(flowContextLocal->processPath, inMetaValues->processPath->data, inMetaValues->processPath->size);

	establishedctx_pushflowestablishedctx(flowContextLocal, sizeof(NF_CALLOUT_FLOWESTABLISHED_INFO));

Exit:
	if (flowContextLocal)
	{
		sl_lock(&g_callouts_flowspinlock, &lh);
		ExFreeToNPagedLookasideList(&g_callouts_flowCtxPacketsLAList, flowContextLocal);
		flowContextLocal = NULL;
		sl_unlock(&lh);
	}

	classifyOut->actionType = FWP_ACTION_PERMIT;
	if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
	{
		classifyOut->flags &= ~FWPS_RIGHT_ACTION_WRITE;
	}
}

NTSTATUS helper_callout_notifyFn_flowEstablished(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ FWPS_FILTER3* filter
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(filterKey);
	return status;
}

// Callouts DataLinkMac Callbacks
VOID helper_callout_classFn_mac(
	_In_ const FWPS_INCOMING_VALUES* inFixedValues,
	_In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	_Inout_opt_ void* layerData,
	_In_opt_ const void* classifyContext,
	_In_ const FWPS_FILTER* filter,
	_In_ UINT64 flowContext,
	_Inout_ FWPS_CLASSIFY_OUT* classifyOut
)
{
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(flowContext);
	UNREFERENCED_PARAMETER(classifyContext);

	PNF_CALLOUT_MAC_INFO pCalloutMacDataInfo = NULL;
	// NET_BUFFER_LIST* pNetBufferList = NULL;

	KLOCK_QUEUE_HANDLE lh;
	NTSTATUS status = STATUS_SUCCESS;
	DWORD IsOutBound = 0;
	NET_BUFFER* netBuffer = NULL;
	UINT32 ipHeaderSize = 0;

	// �رռ�ص�ʱ�򣬲����κβ���
	if (g_monitorflag == FALSE)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	if (!inFixedValues && !inMetaValues &&!layerData)
	{
		status = STATUS_UNSUCCESSFUL;
		goto Exit;
	}

	// ��ֵʧ��
	switch (inFixedValues->layerId)
	{
	case FWPS_LAYER_INBOUND_MAC_FRAME_ETHERNET:
		IsOutBound = 1;
		break;
	case FWPS_LAYER_OUTBOUND_MAC_FRAME_ETHERNET:
		IsOutBound = 2;
		break;
	default:
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		IsOutBound = 0;
		return;
	}
	}

	pCalloutMacDataInfo = ExAllocateFromNPagedLookasideList(&g_callouts_datalinkPacktsList);
	if (!pCalloutMacDataInfo)
	{
		status = STATUS_NO_MEMORY;
		goto Exit;
	}

	RtlSecureZeroMemory(pCalloutMacDataInfo, sizeof(NF_CALLOUT_MAC_INFO));
	
	// DbgBreakPoint();

	pCalloutMacDataInfo->addressFamily =
		(inFixedValues->layerId == FWPS_LAYER_INBOUND_MAC_FRAME_ETHERNET) ? 1 : 2;

	DWORD FWPS_FIELD_IN_OUTBOUND_MAC_LOCAL_ADDRESS = 0;
	DWORD FWPS_FIELD_IN_OUTBOUND_MAC_REMOTE_ADDRESS = 0;
	DWORD FWPS_FIELD_IN_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE = 0;
	switch (IsOutBound)
	{
	case 1:
	{
		FWPS_FIELD_IN_OUTBOUND_MAC_LOCAL_ADDRESS = FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_MAC_LOCAL_ADDRESS;
		FWPS_FIELD_IN_OUTBOUND_MAC_REMOTE_ADDRESS = FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_MAC_REMOTE_ADDRESS;
		FWPS_FIELD_IN_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE = FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE;
	}
	break;
	case 2:
	{
		FWPS_FIELD_IN_OUTBOUND_MAC_LOCAL_ADDRESS = FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_MAC_LOCAL_ADDRESS;
		FWPS_FIELD_IN_OUTBOUND_MAC_REMOTE_ADDRESS = FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_MAC_REMOTE_ADDRESS;
		FWPS_FIELD_IN_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE = FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE;

	}
	break;
	}
	// Mac Packet  Emtpy
	RtlCopyMemory(
		pCalloutMacDataInfo->mac_info.pSourceAddress,
		inFixedValues->incomingValue[FWPS_FIELD_IN_OUTBOUND_MAC_LOCAL_ADDRESS].value.byteArray6,
		sizeof(FWP_BYTE_ARRAY6)
	);
	RtlCopyMemory(pCalloutMacDataInfo->mac_info.pDestinationAddress,
		inFixedValues->incomingValue[FWPS_FIELD_IN_OUTBOUND_MAC_REMOTE_ADDRESS].value.byteArray6,
		sizeof(FWP_BYTE_ARRAY6)
	);
	pCalloutMacDataInfo->mac_info.type = inFixedValues->incomingValue[FWPS_FIELD_IN_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE].value.int16;

	do
	{
		netBuffer = NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData);
		// ����
		if (IsOutBound == 2)
		{
			// MAC FirstNetBuffer
			// ETHERNET_HEADER* ethernet_mac = (ETHERNET_HEADER*)NdisGetDataBuffer(netBuffer, sizeof(ETHERNET_HEADER), NULL, 1, 0);
			NdisAdvanceNetBufferDataStart(netBuffer, sizeof(ETHERNET_HEADER), FALSE, NULL);
		}

		// IP: ip packet: LocalAddr - RemoteAddr - Proto
		IP_HEADER_V4* pIPHeader = (IP_HEADER_V4*)NdisGetDataBuffer(netBuffer, sizeof(struct iphdr), 0, 1, 0);
		if (pIPHeader == NULL)
			break;
		if (pIPHeader->version == 4)
		{
			pCalloutMacDataInfo->ipv4LocalAddr =
				RtlUlongByteSwap(pIPHeader->pSourceAddress);
			pCalloutMacDataInfo->ipv4toRemoteAddr =
				RtlUlongByteSwap(pIPHeader->pDestinationAddress);
			pCalloutMacDataInfo->protocol = pIPHeader->protocol;

			ipHeaderSize = pIPHeader->headerLength * 4;

			// Transport: tcp/udp packet: LocalPort - RemotePort
			switch (pIPHeader->protocol)
			{
			case IPPROTO_TCP:
			{
				NdisAdvanceNetBufferDataStart(netBuffer, ipHeaderSize, FALSE, NULL);
				TCP_HEADER* pTcpHeader = (PTCP_HEADER)NdisGetDataBuffer(netBuffer, sizeof(TCP_HEADER), NULL, sizeof(UINT16), 0);
				if (pTcpHeader)
				{
					pCalloutMacDataInfo->toLocalPort = pTcpHeader->sourcePort;
					pCalloutMacDataInfo->toRemotePort = pTcpHeader->destinationPort;
				}
				NdisRetreatNetBufferDataStart(netBuffer, ipHeaderSize, 0, NULL);
			};
			break;
			case IPPROTO_UDP:
			{
				NdisAdvanceNetBufferDataStart(netBuffer, ipHeaderSize, FALSE, NULL);
				UDP_HEADER* pUdpHeader = (PUDP_HEADER)NdisGetDataBuffer(netBuffer, sizeof(UDP_HEADER), 0, 1, 0);
				if (pUdpHeader)
				{
					pCalloutMacDataInfo->toLocalPort = pUdpHeader->sourcePort;
					pCalloutMacDataInfo->toRemotePort = pUdpHeader->destinationPort;
				}
				NdisRetreatNetBufferDataStart(netBuffer, ipHeaderSize, 0, NULL);
			}
			break;
			}
		}
	} while (FALSE);


	if (IsOutBound == 2)
	{
		// �ָ�ԭʼ��
		NdisRetreatNetBufferDataStart((PNET_BUFFER)netBuffer, sizeof(ETHERNET_HEADER), 0, NULL);
	}

	IsOutBound = 0;

	/*
		Mac Buffer Save
	*/
	pCalloutMacDataInfo->code = NF_DATALINKMAC_LAYER_PACKET;

	// push_data to datalink --> devctrl --> read I/O complate to r3
	datalinkctx_pushdata(pCalloutMacDataInfo, sizeof(NF_CALLOUT_MAC_INFO));

Exit:

	if (pCalloutMacDataInfo)
	{
		sl_lock(&g_callouts_datalinkspinlock, &lh);
		ExFreeToNPagedLookasideList(&g_callouts_datalinkPacktsList, pCalloutMacDataInfo);
		pCalloutMacDataInfo = NULL;
		sl_unlock(&lh);
	}

	classifyOut->actionType = FWP_ACTION_PERMIT;
}

NTSTATUS helper_callout_notifyFn_mac(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ const FWPS_FILTER* filter
)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(filterKey);
	return status;
}

VOID helper_callout_deleteFn_mac(
	IN UINT16 layerId,
	IN UINT32 calloutId,
	IN UINT64 flowContext
)
{
	UNREFERENCED_PARAMETER(layerId);
	UNREFERENCED_PARAMETER(calloutId);
	UNREFERENCED_PARAMETER(flowContext);
}

typedef NTSTATUS (NTAPI* t_FwpsPendClassify0)(
	UINT64 classifyHandle,
	UINT64 filterId,
	UINT32 flags,
	FWPS_CLASSIFY_OUT0* classifyOut
	);
static t_FwpsPendClassify0 _FwpsPendClassify0 = FwpsPendClassify0;

// Callouts TCP Connect Callbacks
VOID helper_callout_classFn_connectredirect(
	IN const FWPS_INCOMING_VALUES* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	IN VOID* packet,
	IN const void* classifyContext,
	IN const FWPS_FILTER* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT* classifyOut)
{
	UNREFERENCED_PARAMETER(flowContext);

	if ((classifyOut->rights & FWPS_RIGHT_ACTION_WRITE) == 0)
	{
		return;
	}

	if (!packet ||
		!classifyContext ||
		(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_FLAGS].value.uint32 & FWP_CONDITION_FLAG_IS_REAUTHORIZE))
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	/*FALSE == g_redirectflag ||*/
	if (FALSE == g_monitorflag)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	PTCPCTX pTcpCtx = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	// ����ṹ�屣������
	pTcpCtx = tcpctxctx_packallocatectx();
	if (pTcpCtx == NULL)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}
	if (inFixedValues->layerId == FWPS_LAYER_ALE_CONNECT_REDIRECT_V4)
	{
		struct sockaddr_in* pAddr = NULL;

		pTcpCtx->layerId = FWPS_LAYER_STREAM_V4;
		pTcpCtx->sendCalloutId = 0;
		pTcpCtx->recvCalloutId =0;
		pTcpCtx->recvProtCalloutId = 0;

		pTcpCtx->transportLayerIdOut = FWPS_LAYER_OUTBOUND_TRANSPORT_V4;
		pTcpCtx->transportCalloutIdOut = 0;
		pTcpCtx->transportLayerIdIn = FWPS_LAYER_INBOUND_TRANSPORT_V4;
		pTcpCtx->transportCalloutIdIn = 0;

		pTcpCtx->ipProto = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_PROTOCOL].value.uint8;
		pTcpCtx->ip_family = AF_INET;

		pAddr = (struct sockaddr_in*)pTcpCtx->localAddr;
		pAddr->sin_family = AF_INET;
		pAddr->sin_addr.S_un.S_addr =
			htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_ADDRESS].value.uint32);
		pAddr->sin_port =
			htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_PORT].value.uint16);

		DbgPrintAddress(AF_INET, pAddr, "localAddr", pTcpCtx->id);

		pAddr = (struct sockaddr_in*)pTcpCtx->remoteAddr;
		pAddr->sin_family = AF_INET;
		pAddr->sin_addr.S_un.S_addr =
			htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_ADDRESS].value.uint32);
		pAddr->sin_port =
			htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_PORT].value.uint16);

		DbgPrintAddress(AF_INET, pAddr, "remoteAddr", pTcpCtx->id);

		pTcpCtx->direction = NF_D_OUT;

		if (inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_ALE_APP_ID].value.byteBlob)
		{
			int offset, len;

			len = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_ALE_APP_ID].value.byteBlob->size;
			if (len > sizeof(pTcpCtx->processName) - 2)
			{
				offset = len - (sizeof(pTcpCtx->processName) - 2);
				len = sizeof(pTcpCtx->processName) - 2;
			}
			else
			{
				offset = 0;
			}
			memcpy(pTcpCtx->processName,
				inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_ALE_APP_ID].value.byteBlob->data + offset,
				len);
		}
	}
	else if (inFixedValues->layerId == FWPS_LAYER_ALE_CONNECT_REDIRECT_V6)
	{
		struct sockaddr_in6* pAddr;

		pTcpCtx->layerId = FWPS_LAYER_STREAM_V6;
		pTcpCtx->sendCalloutId = 0;
		pTcpCtx->recvCalloutId = 0;
		pTcpCtx->recvProtCalloutId = 0;

		pTcpCtx->transportLayerIdOut = FWPS_LAYER_OUTBOUND_TRANSPORT_V6;
		pTcpCtx->transportCalloutIdOut = 0;
		pTcpCtx->transportLayerIdIn = FWPS_LAYER_INBOUND_TRANSPORT_V6;
		pTcpCtx->transportCalloutIdIn = 0;

		pTcpCtx->ipProto = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_PROTOCOL].value.uint8;
		pTcpCtx->ip_family = AF_INET6;

		pAddr = (struct sockaddr_in6*)pTcpCtx->localAddr;
		pAddr->sin6_family = AF_INET6;
		memcpy(&pAddr->sin6_addr,
			inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_LOCAL_ADDRESS].value.byteArray16->byteArray16,
			NF_MAX_IP_ADDRESS_LENGTH);
		pAddr->sin6_port =
			htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_LOCAL_PORT].value.uint16);

		DbgPrintAddress(AF_INET6, pAddr, "localAddr", pTcpCtx->id);

		pAddr = (struct sockaddr_in6*)pTcpCtx->remoteAddr;
		pAddr->sin6_family = AF_INET6;
		memcpy(&pAddr->sin6_addr,
			inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_REMOTE_ADDRESS].value.byteArray16->byteArray16,
			NF_MAX_IP_ADDRESS_LENGTH);
		pAddr->sin6_port =
			htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_REMOTE_PORT].value.uint16);

		DbgPrintAddress(AF_INET6, pAddr, "remoteAddr", pTcpCtx->id);

		if (FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_REMOTE_SCOPE_ID))
		{
			pAddr->sin6_scope_id = inMetaValues->remoteScopeId.Value;
		}

		pTcpCtx->direction = NF_D_OUT;

		if (inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_ALE_APP_ID].value.byteBlob)
		{
			int offset, len;

			len = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_ALE_APP_ID].value.byteBlob->size;
			if (len > sizeof(pTcpCtx->processName) - 2)
			{
				offset = len - (sizeof(pTcpCtx->processName) - 2);
				len = sizeof(pTcpCtx->processName) - 2;
			}
			else
			{
				offset = 0;
			}
			memcpy(pTcpCtx->processName,
				inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_ALE_APP_ID].value.byteBlob->data + offset,
				len);
		}
	}

	/*
	*	����IP�˿ںͽ�������
	*/
	pTcpCtx->processId = inMetaValues->processId;
	if (inMetaValues->processPath && inMetaValues->processPath->size) {
		pTcpCtx->processPathSize = inMetaValues->processPath->size;
		RtlCopyMemory(pTcpCtx->processPath, inMetaValues->processPath->data, inMetaValues->processPath->size);
	}

	/*
	* ������ע����Ҫ������
	*/
	if (classifyOut) {
		RtlCopyMemory(&pTcpCtx->redirectInfo.classifyOut, classifyOut, sizeof(FWPS_CLASSIFY_OUT));
		pTcpCtx->transportEndpointHandle = inMetaValues->transportEndpointHandle;
		pTcpCtx->redirectInfo.filterId = filter->filterId;
	}

	// Craete Inject Handle
#ifdef USE_NTDDI
#if (NTDDI_VERSION >= NTDDI_WIN8)
	status = FwpsRedirectHandleCreate(&g_providerGuid, 0, &pTcpCtx->redirectInfo.redirectHandle);
	if (status != STATUS_SUCCESS)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}
#endif
#endif

	FwpsAcquireClassifyHandle(
		(void*)classifyContext,
		0,
		&(pTcpCtx->redirectInfo.classifyHandle)
	);
	if (status != STATUS_SUCCESS)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		goto Exit;
	}

	status = _FwpsPendClassify0(
		pTcpCtx->redirectInfo.classifyHandle,
		filter->filterId,
		0,
		&pTcpCtx->redirectInfo.classifyOut);
	if (status != STATUS_SUCCESS)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		goto Exit;
	}

	// ����Ϊ������������block
	pTcpCtx->redirectInfo.isPended = TRUE;

	// ���� ���map
	add_tcpHandle(pTcpCtx);
	status = push_tcpRedirectinfo(pTcpCtx, sizeof(TCPCTX));
	if (!NT_SUCCESS(status))
	{
		goto Exit;
	}

	classifyOut->actionType = FWP_ACTION_BLOCK;
	classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	return;

Exit:

	classifyOut->actionType = FWP_ACTION_PERMIT;
	status = STATUS_UNSUCCESSFUL;
}

NTSTATUS helper_callout_notifyFn_connectredirect(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ const FWPS_FILTER* filter
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(filterKey);
	return status;
}

// Callouts Ctrl Manage
NTSTATUS helper_callout_registerCallout(
	IN OUT void* deviceObject,
	IN  FWPS_CALLOUT_CLASSIFY_FN classifyFunction,
	IN  FWPS_CALLOUT_NOTIFY_FN notifyFunction,
	IN  FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN flowDeleteFunction,
	IN  GUID const* calloutKey,
	IN  UINT32 flags,
	OUT UINT32* calloutId
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FWPS_CALLOUT fwpcallout;
	RtlSecureZeroMemory(&fwpcallout, sizeof(FWPS_CALLOUT));

	fwpcallout.calloutKey = *calloutKey;
	fwpcallout.classifyFn = classifyFunction;
	fwpcallout.notifyFn = notifyFunction;
	fwpcallout.flowDeleteFn = flowDeleteFunction;
	fwpcallout.flags = flags;

	status = FwpsCalloutRegister1(deviceObject, (FWPS_CALLOUT1*)&fwpcallout, calloutId);
	return status;
}

NTSTATUS callout_addFlowEstablishedFilter(
	const GUID* calloutKey, 
	const GUID* layer, 
	FWPM_SUBLAYER* subLayer)
{
	NTSTATUS status = STATUS_SUCCESS;
	FWPM_CALLOUT0 fwpcallout0;
	RtlSecureZeroMemory(&fwpcallout0,sizeof(FWPM_CALLOUT0));
	
	FWPM_FILTER fwpfilter;
	RtlSecureZeroMemory(&fwpfilter, sizeof(FWPM_FILTER));

	FWPM_DISPLAY_DATA displayData;
	RtlSecureZeroMemory(&displayData, sizeof(FWPM_DISPLAY_DATA));
	
	FWPM_FILTER_CONDITION filterConditions[3];
	RtlSecureZeroMemory(&filterConditions, sizeof(filterConditions));

	do
	{
		displayData.name = L"Data link Flow Established";
		displayData.description = L"Flow Established Callouts";

		fwpcallout0.displayData = displayData;
		fwpcallout0.applicableLayer = *layer;
		fwpcallout0.calloutKey = *calloutKey;
		fwpcallout0.flags = 0;

		status = FwpmCalloutAdd(g_engineHandle, &fwpcallout0, NULL, NULL);
		if (!NT_SUCCESS(status))
			break;
	
		fwpfilter.subLayerKey = subLayer->subLayerKey;
		fwpfilter.layerKey = *layer;
		fwpfilter.action.calloutKey = *calloutKey;
		fwpfilter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
		fwpfilter.displayData.name = L"Flow Established Callout";
		fwpfilter.displayData.description = L"Flow Established Callout";
		fwpfilter.weight.type = FWP_EMPTY;

		// tcp
		filterConditions[0].conditionValue.type = FWP_UINT8;
		filterConditions[0].conditionValue.uint8 = IPPROTO_TCP;
		filterConditions[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		filterConditions[0].matchType = FWP_MATCH_EQUAL;

		// udp
		filterConditions[1].conditionValue.type = FWP_UINT8;
		filterConditions[1].conditionValue.uint8 = IPPROTO_UDP;
		filterConditions[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		filterConditions[1].matchType = FWP_MATCH_EQUAL;
		
		fwpfilter.filterCondition = filterConditions;
		fwpfilter.numFilterConditions = 2;

		status = FwpmFilterAdd(g_engineHandle, &fwpfilter, NULL, NULL);
		if (!NT_SUCCESS(status))
		{
			break;
		}

	} while (FALSE);

	return status;
}

NTSTATUS callout_addDataLinkMacFilter(
	const GUID* calloutKey,
	const GUID* layer,
	FWPM_SUBLAYER* subLayer,
	const int flaglayer
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FWPM_CALLOUT0 fwpcallout0;
	RtlSecureZeroMemory(&fwpcallout0, sizeof(FWPM_CALLOUT0));

	FWPM_FILTER fwpfilter;
	RtlSecureZeroMemory(&fwpfilter, sizeof(FWPM_FILTER));

	FWPM_DISPLAY_DATA displayData;
	RtlSecureZeroMemory(&displayData, sizeof(FWPM_DISPLAY_DATA));

	FWPM_FILTER_CONDITION filterConditions[3];
	RtlSecureZeroMemory(&filterConditions, sizeof(filterConditions));

	int couts = 0;

	switch (flaglayer)
	{
	case 1:
		displayData.name = L"Sublayer Datalink inbound";
		displayData.description = L"Sublayer Datalink inbound desc";
		fwpfilter.displayData.name = L"Mac inbound Layer Filter";
		fwpfilter.displayData.description = L"Mac inbound Layer Filter desc";
		break;
	case 2:
		displayData.name = L"Sublayer Datalink outbound";
		displayData.description = L"Sublayer Datalink outbound desc";
		fwpfilter.displayData.name = L"Mac outbound Layer Filter";
		fwpfilter.displayData.description = L"Mac outbound Layer Filter desc";
		break;
	case 3:
		break;
	case 4:
		break;
	default:
		break;
	}

	do
	{
		fwpcallout0.displayData = displayData;
		fwpcallout0.applicableLayer = *layer;
		fwpcallout0.calloutKey = *calloutKey;
		fwpcallout0.flags = 0;

		status = FwpmCalloutAdd(g_engineHandle, &fwpcallout0, NULL, NULL);
		if (!NT_SUCCESS(status))
			break;

		fwpfilter.layerKey = *layer;
		fwpfilter.subLayerKey = subLayer->subLayerKey;
		fwpfilter.weight.type = FWP_EMPTY;
		fwpfilter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
		fwpfilter.action.calloutKey = *calloutKey;

		/*
			DataLink Layer
		*/
		filterConditions[couts].conditionValue.type = FWP_UINT16;
		filterConditions[couts].conditionValue.uint16 = NDIS_ETH_TYPE_IPV4;
		filterConditions[couts].fieldKey = FWPM_CONDITION_ETHER_TYPE;
		filterConditions[couts].matchType = FWP_MATCH_EQUAL;
		couts++;

		filterConditions[couts].conditionValue.type = FWP_UINT16;
		filterConditions[couts].conditionValue.uint16 = NDIS_ETH_TYPE_IPV6;
		filterConditions[couts].fieldKey = FWPM_CONDITION_ETHER_TYPE;
		filterConditions[couts].matchType = FWP_MATCH_EQUAL;
		couts++;
		
		fwpfilter.filterCondition = filterConditions;
		fwpfilter.numFilterConditions = couts;

		status = FwpmFilterAdd(g_engineHandle, &fwpfilter, NULL, NULL);
		if (!NT_SUCCESS(status))
		{
			break;
		}

	} while (FALSE);

	return status;
}

NTSTATUS callouts_addFilters()
{
	NTSTATUS status = STATUS_SUCCESS;
	FWPM_CALLOUT0 fwpcallout;
	SECURITY_DESCRIPTOR secur_tor;
	RtlSecureZeroMemory(&fwpcallout, sizeof(FWPM_CALLOUT0));
	RtlSecureZeroMemory(&secur_tor, sizeof(SECURITY_DESCRIPTOR));
	
	status = FwpmTransactionBegin(g_engineHandle, 0);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	// Add Subyler
	FWPM_SUBLAYER subLayer;
	RtlZeroMemory(&subLayer, sizeof(FWPM_SUBLAYER));

	subLayer.subLayerKey = g_sublayerGuid;
	subLayer.displayData.name = L"Mac SubLayer";
	subLayer.displayData.description = L"Established datalink SubLayer";
	subLayer.flags = 0;
	subLayer.weight = FWP_EMPTY;

	do {
		// register Sublayer
		status = FwpmSubLayerAdd(g_engineHandle, &subLayer, NULL);
		if (!NT_SUCCESS(status))
			break;

		status = callout_addFlowEstablishedFilter(
			&g_calloutGuid_flow_established_v4,
			&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
			&subLayer
		);
		if (!NT_SUCCESS(status))
			break;

		status = callout_addFlowEstablishedFilter(
			&g_calloutGuid_flow_established_v6,
			&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6,
			&subLayer
		);
		//if (!NT_SUCCESS(status))
		//	break;
		
		//status = callout_addDataLinkMacFilter(
		//	&g_calloutGuid_inbound_mac_etherent,
		//	&FWPM_LAYER_INBOUND_MAC_FRAME_ETHERNET,
		//	&subLayer,
		//	1
		//);
		//if (!NT_SUCCESS(status))
		//	break;

		//status = callout_addDataLinkMacFilter(
		//	&g_calloutGuid_outbound_mac_etherent,
		//	&FWPM_LAYER_OUTBOUND_MAC_FRAME_ETHERNET,
		//	&subLayer,
		//	2
		//);
		//if (!NT_SUCCESS(status))
		//	break;

		status = callout_addFlowEstablishedFilter(
			&g_calloutGuid_ale_connectredirect_v4,
			&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
			&subLayer
		);

		status = callout_addFlowEstablishedFilter(
			&g_calloutGuid_ale_connectredirect_v6,
			&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
			&subLayer
		);
	
		//// FWPM_LAYER_INBOUND_MAC_FRAME_ETHERNET
		//status = callout_addDataLinkMacFilter(
		//	&g_calloutGuid_inbound_mac_native,
		//	&FWPM_LAYER_INBOUND_MAC_FRAME_NATIVE,
		//	&subLayer,
		//	3);
		//if (!NT_SUCCESS(status))
		//	break;

		//// FWPM_LAYER_OUTBOUND_MAC_FRAME_ETHERNET
		//status = callout_addDataLinkMacFilter(
		//	&g_calloutGuid_outbound_mac_native,
		//	&FWPM_LAYER_OUTBOUND_MAC_FRAME_NATIVE,
		//	&subLayer,
		//	4);
		//if (!NT_SUCCESS(status))
		//	break;

		status = FwpmTransactionCommit(g_engineHandle);
		if (!NT_SUCCESS(status))
			break;

	} while (FALSE);
	
	if (!NT_SUCCESS(status))
	{
		FwpmTransactionAbort(g_engineHandle);
		_Analysis_assume_lock_not_held_(g_engineHandle);
		return status;
	}

	return status;
}

NTSTATUS callouts_registerCallouts(IN OUT void* deviceObject)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = FwpmTransactionBegin(g_engineHandle, 0);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	do
	{
		// FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4
		status = helper_callout_registerCallout(
			deviceObject,
			helper_callout_classFn_flowEstablished,
			helper_callout_notifyFn_flowEstablished,
			NULL,
			&g_calloutGuid_flow_established_v4,
			0,
			&g_calloutId_flow_established_v4
		);
		
		// FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6
		status = helper_callout_registerCallout(
			deviceObject,
			helper_callout_classFn_flowEstablished,
			helper_callout_notifyFn_flowEstablished,
			NULL,
			&g_calloutGuid_flow_established_v6,
			0,
			&g_calloutId_flow_established_v6
		);

		// FWPM_LAYER_INBOUND_MAC_FRAME_ETHERNET
		//status = helper_callout_registerCallout(
		//	deviceObject,
		//	helper_callout_classFn_mac,
		//	helper_callout_notifyFn_mac,
		//	helper_callout_deleteFn_mac,
		//	&g_calloutGuid_inbound_mac_etherent,
		//	0,
		//	&g_calloutId_inbound_mac_etherent
		//);

		//// FWPM_LAYER_OUTBOUND_MAC_FRAME_ETHERNET
		//status = helper_callout_registerCallout(
		//	deviceObject,
		//	helper_callout_classFn_mac,
		//	helper_callout_notifyFn_mac,
		//	helper_callout_deleteFn_mac,
		//	&g_calloutGuid_outbound_mac_etherent,
		//	0,
		//	&g_calloutId_outbound_mac_etherent
		//);

		// FWPM_LAYER_ALE_CONNECT_REDIRECT_V4
		status = helper_callout_registerCallout(
			deviceObject,
			helper_callout_classFn_connectredirect,
			helper_callout_notifyFn_connectredirect,
			NULL,
			&g_calloutGuid_ale_connectredirect_v4,
			0,
			&g_calloutId_ale_connectredirect_v4
		);

		status = helper_callout_registerCallout(
			deviceObject,
			helper_callout_classFn_connectredirect,
			helper_callout_notifyFn_connectredirect,
			NULL,
			&g_calloutGuid_ale_connectredirect_v6,
			0,
			&g_calloutId_ale_connectredirect_v6
		);

		// FWPM_LAYER_INBOUND_MAC_FRAME_NATIVE
		//status = helper_callout_registerCallout(
		//	deviceObject,
		//	NULL,
		//	NULL,
		//	NULL,
		//	&g_calloutGuid_inbound_mac_native,
		//	0,
		//	&g_calloutId_inbound_mac_native
		//);

		//// FWPM_LAYER_OUTBOUND_MAC_FRAME_NATIVE
		//status = helper_callout_registerCallout(
		//	deviceObject,
		//	NULL,
		//	NULL,
		//	NULL,
		//	&g_calloutGuid_outbound_mac_native,
		//	0,
		//	&g_calloutId_outbound_mac_native
		//);

		status = FwpmTransactionCommit(g_engineHandle);
		if (!NT_SUCCESS(status))
			break;

	} while (FALSE);

	if (!NT_SUCCESS(status))
	{
		FwpmTransactionAbort(g_engineHandle);
		_Analysis_assume_lock_not_held_(g_engineHandle);
	}

	return status;
}

NTSTATUS callout_init(PDEVICE_OBJECT deviceObject)
{
	NTSTATUS status = STATUS_SUCCESS;
	DWORD dwStatus = 0;
	FWPM_SESSION0 session;
	RtlSecureZeroMemory(&session, sizeof(FWPM_SESSION0));

	// Create GUID
	ExUuidCreate(&g_calloutGuid_flow_established_v4);
	ExUuidCreate(&g_calloutGuid_flow_established_v6);
	ExUuidCreate(&g_calloutGuid_inbound_mac_etherent);
	ExUuidCreate(&g_calloutGuid_outbound_mac_etherent);
	ExUuidCreate(&g_calloutGuid_inbound_mac_native);
	ExUuidCreate(&g_calloutGuid_outbound_mac_native);
	ExUuidCreate(&g_calloutGuid_ale_connectredirect_v4);
	ExUuidCreate(&g_calloutGuid_ale_connectredirect_v6);
	ExUuidCreate(&g_providerGuid);
	ExUuidCreate(&g_sublayerGuid);

	// Init FlowEstablished 
	KeInitializeSpinLock(&g_callouts_flowspinlock);
	ExInitializeNPagedLookasideList(
		&g_callouts_flowCtxPacketsLAList,
		NULL,
		NULL,
		0,
		sizeof(NF_CALLOUT_FLOWESTABLISHED_INFO),
		'FLHD',
		0
	);

	// Init DataLink
	KeInitializeSpinLock(&g_callouts_datalinkspinlock);
	ExInitializeNPagedLookasideList(
		&g_callouts_datalinkPacktsList,
		NULL,
		NULL,
		0,
		sizeof(NF_CALLOUT_MAC_INFO),
		'FLHD',
		0
	);

	// Open Bfe Engin 
	session.flags = FWPM_SESSION_FLAG_DYNAMIC;
	status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, &session, &g_engineHandle);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	do {
		FWPM_PROVIDER provider;
		RtlZeroMemory(&provider, sizeof(provider));
		provider.displayData.description = NFSDK_PROVIDER_NAME;
		provider.displayData.name = NFSDK_PROVIDER_NAME;
		provider.providerKey = g_providerGuid;
		dwStatus = FwpmProviderAdd(g_engineHandle, &provider, NULL);
		if (dwStatus != 0)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		// add/register callout
		status = callouts_registerCallouts(deviceObject);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		// add sublyer & filterctx 
		status = callouts_addFilters();
		if (!NT_SUCCESS(status))
		{
			break;
		}

		
	} while (FALSE);

	if (!NT_SUCCESS(status))
	{
		callout_free();
	}

	return status;
}

VOID callout_free()
{	
	ExDeleteNPagedLookasideList(&g_callouts_flowCtxPacketsLAList);
	ExDeleteNPagedLookasideList(&g_callouts_datalinkPacktsList);

	// clean guid
	FwpsCalloutUnregisterByKey(&g_calloutGuid_flow_established_v4);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_flow_established_v6);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_inbound_mac_etherent);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_outbound_mac_etherent);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_ale_connectredirect_v4);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_ale_connectredirect_v6);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_inbound_mac_native);
	FwpsCalloutUnregisterByKey(&g_calloutGuid_outbound_mac_native);

	// clean id
	FwpsCalloutUnregisterById(g_calloutId_flow_established_v4);
	FwpsCalloutUnregisterById(g_calloutId_flow_established_v6);
	FwpsCalloutUnregisterById(g_calloutId_inbound_mac_etherent);
	FwpsCalloutUnregisterById(g_calloutId_outbound_mac_etherent);
	FwpsCalloutUnregisterById(g_calloutId_inbound_mac_native);
	FwpsCalloutUnregisterById(g_calloutId_outbound_mac_native);
	FwpsCalloutUnregisterById(g_calloutId_ale_connectredirect_v4);
	FwpsCalloutUnregisterById(g_calloutId_ale_connectredirect_v6);

	// clean SubLayer
	FwpmSubLayerDeleteByKey(g_engineHandle, &g_sublayerGuid);

	// clean
	FwpmProviderContextDeleteByKey(g_engineHandle, &g_providerGuid);

	if (g_engineHandle)
	{
		FwpmEngineClose(g_engineHandle);
		g_engineHandle = NULL;
	}
}