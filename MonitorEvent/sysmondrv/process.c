#include "public.h"
#include "process.h"
#include "devctrl.h"
#include "rProcess.h"
#include "kflt.h"
#include "utiltools.h"

#include <ntddk.h>
#include <ndis.h>

static  BOOLEAN     g_proc_monitorprocess = FALSE;
static  KSPIN_LOCK  g_proc_monitorlock = 0;

static  BOOLEAN     g_proc_ips_monitorprocess = FALSE;
static  KSPIN_LOCK  g_proc_ips_monitorlock = 0;

static  int         g_proc_ipsmod = 0;
static  KSPIN_LOCK  g_proc_ipslock = 0;

static KSPIN_LOCK               g_processlock = 0;
static NPAGED_LOOKASIDE_LIST    g_processList;
static PROCESSDATA              g_processQueryhead;

static VOID Process_NotifyProcessEx(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    UNREFERENCED_PARAMETER(ProcessId);
    UNREFERENCED_PARAMETER(Process);
    if (KeGetCurrentIrql() > APC_LEVEL)
        return;

    // �رռ��
    if (FALSE == g_proc_monitorprocess && FALSE == g_proc_ips_monitorprocess)
    {
        return;
    }

    NTSTATUS status = STATUS_SUCCESS;
    WCHAR path[260 * 2] = { 0 };
    BOOLEAN QueryPathStatus = FALSE;
    if (QueryProcessNamePath((DWORD)ProcessId, path, sizeof(path)))
        QueryPathStatus = TRUE;
    if (g_proc_ips_monitorprocess && g_proc_ipsmod && CreateInfo && QueryPathStatus)
    {// Ips
        const BOOLEAN nRet = rProcess_IsIpsProcessNameInList(path);
        //PHADES_NOTIFICATION  notification = NULL;
        //do {
        //    int replaybuflen = sizeof(HADES_REPLY);
        //    int sendbuflen = sizeof(HADES_NOTIFICATION);
        //    notification = (char*)ExAllocatePoolWithTag(NonPagedPool, sendbuflen, 'IPSP');
        //    if (!notification)
        //        break;
        //    RtlZeroMemory(notification, sendbuflen);
        //    notification->CommandId = 1; // MINIPORT_IPS_PROCESSSTART
        //    processinfo.parentprocessid = CreateInfo->ParentProcessId;
        //    RtlCopyMemory(&notification->Contents, &processinfo, sizeof(PROCESSINFO));
        //    // �ȴ��û�����
        //    NTSTATUS nSendRet = Fsflt_SendMsg(notification, sendbuflen, notification, &replaybuflen);
        //    // ����Error: ���ݻ���������,��ʵ�Ѿ�������
        //    const DWORD  ReSafeToOpen = ((PHADES_REPLY)notification)->SafeToOpen;
        //    // ��ֹ
        //    if ((1 == ReSafeToOpen) || (3 == ReSafeToOpen))
        //        CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
        //} while (FALSE);
        //if (notification)
        //{
        //    ExFreePoolWithTag(notification, 'IPSP');
        //    notification = NULL;
        //} 
        if (!nRet && g_proc_ipsmod == 1)
            CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
        else if (nRet && g_proc_ipsmod == 2)
            CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
    }
    if (FALSE == g_proc_monitorprocess)
        return;

    PROCESSINFO processinfo;
    RtlZeroMemory(&processinfo, sizeof(PROCESSINFO));
    processinfo.pid = (DWORD)ProcessId;
    if (QueryPathStatus)
        RtlCopyMemory(processinfo.queryprocesspath, path, sizeof(WCHAR) * 260);

    KLOCK_QUEUE_HANDLE lh;
    PROCESSBUFFER* pinfo = (PROCESSBUFFER*)Process_PacketAllocate(sizeof(PROCESSINFO));
    if (!pinfo)
        return;
    if (NULL == CreateInfo)
    {
        processinfo.endprocess = 0;
        pinfo->dataLength = sizeof(PROCESSINFO);
        RtlCopyMemory(pinfo->dataBuffer, &processinfo, sizeof(PROCESSINFO));
        sl_lock(&g_processQueryhead.process_lock, &lh);
        InsertHeadList(&g_processQueryhead.process_pending, &pinfo->pEntry);
        sl_unlock(&lh);
        return;
    }
    else
        processinfo.endprocess = 1;
    if (CreateInfo->ImageFileName && (CreateInfo->ImageFileName->Length < 260 * 2)) 
        RtlCopyMemory(processinfo.processpath, CreateInfo->ImageFileName->Buffer, CreateInfo->ImageFileName->Length);
    if (CreateInfo->CommandLine && (CreateInfo->CommandLine->Length < 260 * 2))
        RtlCopyMemory(processinfo.commandLine, CreateInfo->CommandLine->Buffer, CreateInfo->CommandLine->Length);
    processinfo.parentprocessid = CreateInfo->ParentProcessId;

    pinfo->dataLength = sizeof(PROCESSINFO);
    RtlCopyMemory(pinfo->dataBuffer, &processinfo, sizeof(PROCESSINFO));

    sl_lock(&g_processQueryhead.process_lock, &lh);
    InsertHeadList(&g_processQueryhead.process_pending, &pinfo->pEntry);
    sl_unlock(&lh);
    devctrl_pushinfo(NF_PROCESS_INFO);
    return;
}

NTSTATUS Process_Init(void) {

    sl_init(&g_processlock);
    sl_init(&g_proc_monitorlock);
    sl_init(&g_proc_ipslock);
    sl_init(&g_proc_ips_monitorlock);

    rProcess_IpsInit();

    VerifiExInitializeNPagedLookasideList(
        &g_processList,
        NULL,
        NULL,
        0,
        sizeof(PROCESSBUFFER),
        'PRMM',
        0
    );

    sl_init(&g_processQueryhead.process_lock);
    InitializeListHead(&g_processQueryhead.process_pending);

    // See: Available starting with Windows Vista with SP1 and Windows Server 2008.
    // Msdn: https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex
	PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)Process_NotifyProcessEx, FALSE);
    return STATUS_SUCCESS;
}
void Process_Free(void)
{
    // Set Close Monitro
    Process_Clean();
    ExDeleteNPagedLookasideList(&g_processList);
    PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)Process_NotifyProcessEx, TRUE);
}
void Process_Clean(void)
{
    KLOCK_QUEUE_HANDLE lh;
    PROCESSBUFFER* pData = NULL;
    int lock_status = 0;

    // Ips Rule Name
    rProcess_IpsClean();

    try
    {
        // Distable ProcessMon
        sl_lock(&g_processQueryhead.process_lock, &lh);
        lock_status = 1;
        // 4/24Ī����BUG���������ڴ棬process_pending���ݣ������ݻ�������
        while (!IsListEmpty(&g_processQueryhead.process_pending))
        {
            pData = (PROCESSBUFFER*)RemoveHeadList(&g_processQueryhead.process_pending);
            sl_unlock(&lh);
            lock_status = 0;
            Process_PacketFree(pData);
            pData = NULL;
            sl_lock(&g_processQueryhead.process_lock, &lh);
            lock_status = 1;
        }
        sl_unlock(&lh);
        lock_status = 0;
    }
    finally {
        if (1 == lock_status)
            sl_unlock(&lh);
    }

}

void Process_SetMonitor(BOOLEAN code)
{// �ɼ�����
    KLOCK_QUEUE_HANDLE lh;

    sl_lock(&g_proc_monitorlock, &lh);
    g_proc_monitorprocess = code;
    sl_unlock(&lh);
}
void Process_SetIpsMonitor(BOOLEAN code)
{// IPS����
    KLOCK_QUEUE_HANDLE lh;

    sl_lock(&g_proc_ips_monitorlock, &lh);
    g_proc_ips_monitorprocess = code;
    sl_unlock(&lh);
}
void Process_SetIpsModEx(const int mods)
{// ģʽ����
    KLOCK_QUEUE_HANDLE lh;

    sl_lock(&g_proc_ipslock, &lh);
    g_proc_ipsmod = mods;
    sl_unlock(&lh);
}
NTSTATUS Process_SetIpsMod(PIRP irp, PIO_STACK_LOCATION irpSp)
{
    NTSTATUS status = STATUS_SUCCESS;
    do {
        PVOID inputBuffer = irp->AssociatedIrp.SystemBuffer;
        ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
        ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
        if (NULL == inputBuffer || inputBufferLength < sizeof(DWORD32))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        const DWORD32 mods = *(PDWORD32)inputBuffer;
        Process_SetIpsModEx(mods);
    } while (FALSE);

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

PROCESSDATA* processctx_get()
{
    return &g_processQueryhead;
}
PROCESSBUFFER* Process_PacketAllocate(const int lens)
{
    PROCESSBUFFER* pProcessData = NULL;
    pProcessData = (PROCESSBUFFER*)ExAllocateFromNPagedLookasideList(&g_processList);
    if (!pProcessData)
        return NULL;

    memset(pProcessData, 0, sizeof(PROCESSBUFFER));

    if (lens > 0)
    {
        pProcessData->dataBuffer = (char*)VerifiExAllocatePoolTag(lens, 'PRMM');
        if (!pProcessData->dataBuffer)
        {
            ExFreeToNPagedLookasideList(&g_processList, pProcessData);
            return FALSE;
        }
    }
    return pProcessData;
}
void Process_PacketFree(PROCESSBUFFER* packet)
{
    if (packet->dataBuffer)
    {
        free_np(packet->dataBuffer);
        packet->dataBuffer = NULL;
    }
    ExFreeToNPagedLookasideList(&g_processList, packet);
}



