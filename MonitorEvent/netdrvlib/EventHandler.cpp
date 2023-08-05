#include <sysinfo.h>
#include "NetApi.h"
#include "nfevents.h"
#include "EventHandler.h"
#include "tcpctx.h"
#include "establishedctx.h"
#include "datalinkctx.h"
#include "CodeTool.h"
#include <mutex>
#include <map>
#include <vector>

typedef struct _PROCESS_INFO
{
	UINT64 processId;
	WCHAR  processPath[MAX_PATH * 2];
	void clear()
	{
		processId = 0;
		RtlSecureZeroMemory(processPath, sizeof(processPath));
	}
}PROCESS_INFO, * PPROCESS_INFO;

static std::mutex g_mutx;
static std::map<int, NF_CALLOUT_FLOWESTABLISHED_INFO> g_flowestablished_map;

static std::vector<int>			ids_destinationport;
static std::vector<ULONGLONG>	ids_destinationaddress;
static std::vector<ULONGLONG>	ids_destinationaddressport;

void EventHandler::EstablishedPacket(const char* buf, int len)
{
	NF_CALLOUT_FLOWESTABLISHED_INFO flowestablished_processinfo;
	RtlSecureZeroMemory(&flowestablished_processinfo, sizeof(NF_CALLOUT_FLOWESTABLISHED_INFO));
	RtlCopyMemory(&flowestablished_processinfo, buf, len);

	/*
		TCP - UDP ��ͬЭ����ͬ�˿ڽ����ǣ���Ϊ������Ҫ�������еİ�
	*/
	DWORD keyLocalPort = flowestablished_processinfo.toLocalPort;
	switch (flowestablished_processinfo.protocol)
	{
	case IPPROTO_TCP:
		keyLocalPort += 1000000;
		break;
	case IPPROTO_UDP:
		keyLocalPort += 2000000;
		break;
	default:
	{
		OutputDebugString(L"Other Protocol Erro");
	}
	}
	g_mutx.lock();
	g_flowestablished_map[keyLocalPort] = flowestablished_processinfo;
	g_mutx.unlock();

	//// test api �����Ƿ���Դ�map��ȡ����
	//PROCESS_INFO processinfo = { 0, };
	//nf_getprocessinfo(&flowestablished_processinfo.ipv4LocalAddr, flowestablished_processinfo.toLocalPort, flowestablished_processinfo.protocol, &processinfo);
	//processinfo.processId;
	//processinfo.processPath;

	// test path
	std::wstring wsinfo;
	WCHAR info[MAX_PATH] = { 0, };
	// swprintf(str, 100, L"%ls%d is %d", L"The half of ", 80, 80 / 2);
	swprintf(info, MAX_PATH, L"Locate: 0x%d:%d -> remote: 0x%d:%d type: %d", \
		flowestablished_processinfo.ipv4LocalAddr, flowestablished_processinfo.toLocalPort, \
		flowestablished_processinfo.ipv4toRemoteAddr, flowestablished_processinfo.toRemotePort, \
		flowestablished_processinfo.protocol
	);
	wsinfo = flowestablished_processinfo.processPath;
	wsinfo += L"\r\n";
	wsinfo += info;
	OutputDebugString(wsinfo.data());
}

void EventHandler::DatalinkPacket(const char* buf, int len)
{
	NF_CALLOUT_MAC_INFO datalink_netinfo;
	RtlSecureZeroMemory(&datalink_netinfo, sizeof(NF_CALLOUT_MAC_INFO));
	RtlCopyMemory(&datalink_netinfo, buf, len);

	OutputDebugString(L"-------------------------------------");
	OutputDebugStringA((LPCSTR)datalink_netinfo.mac_info.pSourceAddress);
	OutputDebugStringA((LPCSTR)datalink_netinfo.mac_info.pDestinationAddress);
	OutputDebugString(L"-------------------------------------");
}

void EventHandler::TcpredirectPacket(const char* buf, int len)
{
	PTCPCTX redirect_info;
	RtlSecureZeroMemory(&redirect_info, sizeof(NF_CALLOUT_FLOWESTABLISHED_INFO));
	RtlCopyMemory(&redirect_info, buf, len);

	/*
		1 - ��Ҫ�أ�Ŀ�� port ���� ip
		2 - ˫Ҫ�أ�Ŀ��ip:port
		3 - �ض����־λ - ��ʱ������
	*/
	size_t i = 0;
	// if (redirect_info.addressFamily == AF_INET)
	{
		switch (0)
		{
		case 1:
		{
		}
		break;
		case 2:
		{

		}
		break;
		default:
			break;
		}
	}

	// ������ע��ȥ

}

/*
	@ ����1 ipv4 address
	@ ����2 ���ض˿�
	@ ����3 Э��
	@ ����4 ����ָ��
*/
int NetNdrGetProcessInfoEx(unsigned int* Locaaddripv4, unsigned long localport, int protocol, void* pGetbuffer)
{
	// -1 ��������
	if (!Locaaddripv4 && (localport <= 0) && !pGetbuffer && !protocol)
		return  -1;

	switch (protocol)
	{
	case IPPROTO_TCP:
		localport += 1000000;
		break;
	case IPPROTO_UDP:
		localport += 2000000;
		break;
	}

	try
	{
		PPROCESS_INFO processinf = NULL;
		processinf = (PPROCESS_INFO)pGetbuffer;
		auto mapiter = g_flowestablished_map.find(localport);
		// -3 find failuer not`t processinfo
		if (mapiter == g_flowestablished_map.end())
			return -3;
		processinf->processId = mapiter->second.processId;
		//RtlCopyMemory(processinf->processPath, mapiter->second.processPath, mapiter->second.processPathSize);

		WCHAR ntPath[MAX_PATH] = { 0 };
		CodeTool::DeviceDosPathToNtPath(mapiter->second.processPath, ntPath);
		RtlCopyMemory(processinf->processPath, ntPath, sizeof(ntPath));
		return 1;
	}
	catch (const std::exception&)
	{
		// �쳣
		return -4;
	}
}