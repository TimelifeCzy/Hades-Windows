/*
	- 2018 year:
	-- nf_EnumProcessHandle
	-- nf_EnumModuleByPid
	ʹ��ZhuHuiBeiShaDiaoARK-master����: https://github.com/bekdepo/ZhuHuiBeiShaDiaoARK

	- 2021 year:
	-- nf_KillProcess�ο���
	https://blog.csdn.net/Simon798/article/details/106939256

	-- nf_DumpProcesss�ں�ֻ��Ҫ��ѯ������ַ�ʹ�С�������޸�PE������Ӧ�ò���С�
*/
#include "public.h"
#include "sysprocessinfo.h"

typedef NTSTATUS(__fastcall* PSPTERMINATETHREADBYPOINTER)
(
	IN PETHREAD Thread,
	IN NTSTATUS ExitStatus,
	IN BOOLEAN DirectTerminate
	);
PSPTERMINATETHREADBYPOINTER PspTerminateThreadByPointer = NULL;

typedef NTSTATUS(*PfnZwQueryInformationProcess)(
	_In_      HANDLE           ProcessHandle,
	_In_      PROCESSINFOCLASS ProcessInformationClass,
	_Out_     PVOID            ProcessInformation,
	_In_      ULONG            ProcessInformationLength,
	_Out_opt_ PULONG           ReturnLength
);
PfnZwQueryInformationProcess ZwQueryInformationProcess;

/*
	��Ҫ�Ż� ֻ��Ҫ����ʼ��һ�ξͺ�
	�����process.c .h��ģ�鼯�ɵ�һ���������棬���ô���
*/
void InitGloableFunction_Process1()
{
	UNICODE_STRING UtrZwQueryInformationProcess =
		RTL_CONSTANT_STRING(L"ZwQueryInformationProcess");
	ZwQueryInformationProcess =
		(PfnZwQueryInformationProcess)MmGetSystemRoutineAddress(&UtrZwQueryInformationProcess);
}
// ָ���ڴ������������ɨ��
PVOID SearchMemory(PVOID pStartAddress, PVOID pEndAddress, PUCHAR pMemoryData, ULONG ulMemoryDataSize)
{
	PVOID pAddress = NULL;
	PUCHAR i = NULL;
	ULONG m = 0;
	// ɨ���ڴ�
	for (i = (PUCHAR)pStartAddress; i < (PUCHAR)pEndAddress; i++)
	{
		// �ж�������
		for (m = 0; m < ulMemoryDataSize; m++)
		{
			if (*(PUCHAR)(i + m) != pMemoryData[m])
			{
				break;
			}
		}
		// �ж��Ƿ��ҵ�����������ĵ�ַ
		if (m >= ulMemoryDataSize)
		{
			// �ҵ�������λ��, ��ȡ���������������һ��ַ
			pAddress = (PVOID)(i + ulMemoryDataSize);
			break;
		}
	}
	return pAddress;
}
PVOID SearchPspTerminateThreadByPointer(PUCHAR pSpecialData, ULONG ulSpecialDataSize)
{
	UNICODE_STRING ustrFuncName;
	PVOID pAddress = NULL;
	LONG lOffset = 0;
	PVOID pPsTerminateSystemThread = NULL;
	PVOID pPspTerminateThreadByPointer = NULL;
	// �Ȼ�ȡ PsTerminateSystemThread ������ַ
	RtlInitUnicodeString(&ustrFuncName, L"PsTerminateSystemThread");
	pPsTerminateSystemThread = MmGetSystemRoutineAddress(&ustrFuncName);
	if (NULL == pPsTerminateSystemThread)
	{
		return pPspTerminateThreadByPointer;
	}
	// Ȼ��, ���� PspTerminateThreadByPointer ������ַ
	pAddress = SearchMemory(pPsTerminateSystemThread,
		(PVOID)((PUCHAR)pPsTerminateSystemThread + 0xFF),
		pSpecialData, ulSpecialDataSize);
	if (NULL == pAddress)
	{
		return pPspTerminateThreadByPointer;
	}
	// �Ȼ�ȡƫ��, �ټ����ַ
	lOffset = *(PLONG)pAddress;
	pPspTerminateThreadByPointer = (PVOID)((PUCHAR)pAddress + sizeof(LONG) + lOffset);
	return pPspTerminateThreadByPointer;
}
PVOID GetPspLoadImageNotifyRoutine()
{
	PVOID pPspTerminateThreadByPointerAddress = NULL;
	RTL_OSVERSIONINFOW osInfo = { 0 };
	UCHAR pSpecialData[50] = { 0 };
	ULONG ulSpecialDataSize = 0;
	// ��ȡϵͳ�汾��Ϣ, �ж�ϵͳ�汾
	RtlGetVersion(&osInfo);
	if (6 == osInfo.dwMajorVersion)
	{
		if (1 == osInfo.dwMinorVersion)
		{
			// Win7
#ifdef _WIN64
			// 64 λ
			// E8
			pSpecialData[0] = 0xE8;
			ulSpecialDataSize = 1;
#else
			// 32 λ
			// E8
			pSpecialData[0] = 0xE8;
			ulSpecialDataSize = 1;
#endif    
		}
		else if (2 == osInfo.dwMinorVersion)
		{
			// Win8
#ifdef _WIN64
			// 64 λ
#else
			// 32 λ
#endif
		}
		else if (3 == osInfo.dwMinorVersion)
		{
			// Win8.1
#ifdef _WIN64
			// 64 λ
			// E9
			pSpecialData[0] = 0xE9;
			ulSpecialDataSize = 1;
#else
			// 32 λ
			// E8
			pSpecialData[0] = 0xE8;
			ulSpecialDataSize = 1;
#endif            
		}
	}
	else if (10 == osInfo.dwMajorVersion)
	{
		// Win10
#ifdef _WIN64
		// 64 λ
		// E9
		pSpecialData[0] = 0xE9;
		ulSpecialDataSize = 1;
#else
		// 32 λ
		// E8
		pSpecialData[0] = 0xE8;
		ulSpecialDataSize = 1;
#endif
	}
	// �����������ȡ��ַ
	pPspTerminateThreadByPointerAddress = SearchPspTerminateThreadByPointer(pSpecialData, ulSpecialDataSize);
	return pPspTerminateThreadByPointerAddress;
}

// PHANDLE_INFO g_pHandleInfo = NULL;

BOOLEAN QueryProcessNamePath1(__in DWORD pid, __out PWCHAR path, __in DWORD pathlen);
VOID CharToWchar(PCHAR src, PWCHAR dst)
{
	UNICODE_STRING uString;
	ANSI_STRING aString;
	RtlInitAnsiString(&aString, src);
	RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);
	wcscpy(dst, uString.Buffer);
	RtlFreeUnicodeString(&uString);
}
NTSTATUS GetProcessPathByPid(HANDLE pid, WCHAR* szProcessName)
{

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PEPROCESS Process = NULL;
	PUNICODE_STRING szProcessPath;

	if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &Process)))
	{
		return status;
	}

	status = SeLocateProcessImageName(Process, &szProcessPath);

	if (!NT_SUCCESS(status))
	{
		ObDereferenceObject(Process);
		return status;
	}


	//memcpy(szProcessName,szProcessPath->Buffer,szProcessPath->Length);
	//wcsncpy(szProcessName,szProcessPath->Buffer,szProcessPath->Length*2);
	__try {
		RtlCopyMemory(szProcessName, szProcessPath->Buffer, szProcessPath->Length * 2);
	}
	__except (1)
	{
		DbgPrint("GetProcessPathByPid error !\n");
	}


	ExFreePool(szProcessPath);

	ObDereferenceObject(Process);

	return STATUS_SUCCESS;

}

ULONG_PTR nf_GetProcessInfo(int Enumbool, HANDLE pid, PHANDLE_INFO pOutBuffer)
{
	PVOID pBuffer = NULL;
	ULONG BufferSize = 0x20000, rtl = 0;
	NTSTATUS Status;
	NTSTATUS ns = STATUS_SUCCESS;
	ULONG64 i = 0;
	ULONG64 qwHandleCount;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO* p;
	OBJECT_BASIC_INFORMATION BasicInfo = { 0 };
	POBJECT_NAME_INFORMATION pNameInfo;
	POBJECT_TYPE_INFORMATION pTypeInfo;
	PROCESS_BASIC_INFORMATION	processBasic = { 0, };
	ULONG ulProcessID;
	HANDLE hProcess;
	HANDLE hHandle;
	HANDLE hDupObj;
	CLIENT_ID cid;
	OBJECT_ATTRIBUTES oa;
	ULONG_PTR Count = 0;
	char* szProcessName = NULL;

	InitGloableFunction_Process1();
	if (!ZwQueryInformationProcess)
		return Count;
	pBuffer = VerifiExAllocatePoolTag(BufferSize, MEM_TAG);
	if (!pBuffer || (pBuffer == NULL))
		return Count;
	memset(pBuffer, 0, BufferSize);
	Status = ZwQuerySystemInformation(16, pBuffer, BufferSize, 0);	//SystemHandleInformation=16
	while (Status == 0xC0000004)	//STATUS_INFO_LENGTH_MISMATCH
	{
		free_np(pBuffer);
		pBuffer = NULL;
		BufferSize = BufferSize * 2;
		pBuffer = VerifiExAllocatePoolTag(BufferSize, MEM_TAG);
		memset(pBuffer, 0, BufferSize);
		Status = ZwQuerySystemInformation(16, pBuffer, BufferSize, 0);
	}
	if (!NT_SUCCESS(Status))
		return 0x88888888;
	
	qwHandleCount = ((SYSTEM_HANDLE_INFORMATION*)pBuffer)->NumberOfHandles;
	p = (SYSTEM_HANDLE_TABLE_ENTRY_INFO*)((SYSTEM_HANDLE_INFORMATION*)pBuffer)->Handles;
	memset(pOutBuffer, 0, 1024 * sizeof(HANDLE_INFO) * 2);

	//ENUM HANDLE PROC
	for (i = 0; i < qwHandleCount; i++)
	{
		// ö�ٽ���
		if (p[i].ObjectTypeIndex != 7)
			continue;

		// Enumbool��־: PID�����Ƿ���Ч
		if (Enumbool)
		{
			(ULONG)pid = (ULONG)p[i].UniqueProcessId;
		}

		if ((ULONG)pid == (ULONG)p[i].UniqueProcessId)
		{			
			ulProcessID = (ULONG)p[i].UniqueProcessId;
			cid.UniqueProcess = (HANDLE)ulProcessID;
			cid.UniqueThread = (HANDLE)0;
			hHandle = (HANDLE)p[i].HandleValue;
			InitializeObjectAttributes(&oa, NULL, 0, NULL, NULL);
			ns = ZwOpenProcess(&hProcess, PROCESS_DUP_HANDLE, &oa, &cid);
			if (!NT_SUCCESS(ns))
				continue;
			ns = ZwDuplicateObject(hProcess, hHandle, NtCurrentProcess(), &hDupObj, PROCESS_ALL_ACCESS, 0, DUPLICATE_SAME_ACCESS);
			if (!NT_SUCCESS(ns))
			{
				if (ns == 0xc00000bb)
				{
					//KdPrint(( "This is EtwRegistration ZwDuplicateObject Fail Code :0x%x",ns ));
					pOutBuffer[Count].GrantedAccess = p[i].GrantedAccess;
					pOutBuffer[Count].HandleValue = p[i].HandleValue;
					//wcsncpy(pOutBuffer[Count].HandleName, L" ", wcslen(L" "));
					pOutBuffer[Count].Object = (ULONG64)p[i].Object;
					pOutBuffer[Count].ObjectTypeIndex = p[i].ObjectTypeIndex;
					// wcsncpy(pOutBuffer[Count].TypeName, L"EtwRegistration", wcslen(L"EtwRegistration"));
					pOutBuffer[Count].ReferenceCount = 0;
					wcsncpy(pOutBuffer[Count].ProcessName, L"System", wcslen(L"System"));
					Count++;
				}
				continue;
			}

			// ��ȡ����ʵ��Pid
			ZwQueryInformationProcess(hDupObj, ProcessBasicInformation, &processBasic, sizeof(PROCESS_BASIC_INFORMATION), NULL);
			pOutBuffer[Count].ProcessId = processBasic.UniqueProcessId;

			// get basic information
			//ZwQueryObject(hDupObj, ObjectBasicInformation, &BasicInfo, sizeof(OBJECT_BASIC_INFORMATION), NULL);
			//pTypeInfo = ExAllocatePool(PagedPool, 1024);
			//RtlZeroMemory(pTypeInfo, 1024);
			//ZwQueryObject(hDupObj, ObjectTypeInformation, pTypeInfo, 1024, NULL);

			//// get name information
			//pNameInfo = ExAllocatePoolWithTag(PagedPool, 1024, 'ONON');
			//RtlZeroMemory(pNameInfo, 1024);
			//ZwQueryObject(hDupObj, (OBJECT_INFORMATION_CLASS)ObjectNameInformation1, pNameInfo, 1024, &rtl);

			// get information and close handle
			//UnicodeStringToCharArray(&(pNameInfo->Name),szFile);	
			ZwClose(hDupObj);
			ZwClose(hProcess);

			// Name
			//if (p[i].ObjectTypeIndex == 7)//Process
			//{
			//	szProcessName = (PCHAR)PsGetProcessImageFileName((PEPROCESS)p[i].Object);
			//	CharToWchar(szProcessName, pOutBuffer[Count].ProcessName);
			//}
			//else if (p[i].ObjectTypeIndex == 8)//Thread
			//{
			//	szProcessName = (PCHAR)PsGetProcessImageFileName(IoThreadToProcess((PETHREAD)p[i].Object));
			//	CharToWchar(szProcessName, pOutBuffer[Count].ProcessName);
			//}

			// ProcessPath
			QueryProcessNamePath1((DWORD)pOutBuffer[Count].ProcessId, pOutBuffer[Count].ProcessPath, 256 * 2);

			// Process_KernelData
			pOutBuffer[Count].ObjectTypeIndex = p[i].ObjectTypeIndex;
			pOutBuffer[Count].ReferenceCount = BasicInfo.ReferenceCount;
			pOutBuffer[Count].GrantedAccess = BasicInfo.DesiredAccess;
			pOutBuffer[Count].HandleValue = p[i].HandleValue;
			pOutBuffer[Count].Object = (ULONG64)p[i].Object;
	/*		wcsncpy(pOutBuffer[Count].HandleName, pNameInfo->Name.Buffer, pNameInfo->Name.Length * 2);
			wcsncpy(pOutBuffer[Count].TypeName, pTypeInfo->TypeName.Buffer, pTypeInfo->TypeName.Length * 2);*/

			// ExFreePool(pNameInfo);
			// ExFreePool(pTypeInfo);
			Count++;

			// Max_Process
			if (Count > 2000)
				break;
		}
	}
	pOutBuffer[0].CountNum = Count;
	return Count;
}
VOID nf_EnumModuleByPid(ULONG pid, PPROCESS_MOD ModBuffer)
{
	PPEB Peb = NULL, Ldr = 0, tmp = 0;
	PLIST_ENTRY ModListHead = 0;
	PLIST_ENTRY Module = 0;
	KAPC_STATE ks = { 0 };
	ULONG count = 0;

	//����LDR-NATIVE
#ifdef AMD64
	const int LdrInPebOffset = 0x18;
	const int ModListInPebOffset = 0x10;
#else
	const int LdrInPebOffset = 0xC;
	const int ModListInPebOffset = 0xC;
#endif

	PEPROCESS eprocess = NULL;
	if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &eprocess)))
		return;

	Peb = PsGetProcessPeb(eprocess);
	if (!Peb)
		return;

	KeStackAttachProcess(eprocess, &ks);
	__try
	{
		Ldr = (SIZE_T)Peb + (SIZE_T)LdrInPebOffset;
		ProbeForRead((CONST PVOID)Ldr, sizeof(void*), sizeof(void*));
		ModListHead = (PLIST_ENTRY)(*(PULONG64)Ldr + ModListInPebOffset);
		ProbeForRead((CONST PVOID)ModListHead, sizeof(void*), sizeof(void*));
		Module = ModListHead->Flink;
		while (ModListHead != Module)
		{

			// WCHAR ModuleName[260] = { 0 };
			memcpy(ModBuffer[count].FullDllName,
				((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Buffer,
				((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Length);
			ModBuffer[count].DllBase = ((PLDR_DATA_TABLE_ENTRY)Module)->DllBase;

			count++;

			if (count > 2000)
				break;

			//next module
			Module = Module->Flink;
			ProbeForRead((CONST PVOID)Module, 10 * sizeof(void*), sizeof(void*));
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DbgPrint("[EnumModule64]__except (EXCEPTION_EXECUTE_HANDLER)");
	}
#ifdef AMD64
	//����LDR-32 ö��32λ���̵�32λDLL
	tmp = (SIZE_T)PsGetProcessWow64Process(eprocess);
	if (tmp)
	{
		SIZE_T peb, ldr;
		PLIST_ENTRY32 ModListHead32 = 0;
		PLIST_ENTRY32 Module32 = 0;
		peb = tmp;
		__try
		{
			//peb->ldr
			ldr = *(PULONG)(peb + 0xC);//OFFSET_PEB_LDR
			ProbeForRead((CONST PVOID)ldr, 4, 4);
			//peb->Ldr->InLoadOrderModuleList
			ModListHead32 = (PLIST_ENTRY32)(*(PULONG)(ldr + 0xC));//OFFSET_LDR_InLoadOrderModuleList
			ProbeForRead((CONST PVOID)ModListHead32, 4, 4);
			//peb->Ldr->InLoadOrderModuleList->Flink
			Module32 = (PLIST_ENTRY32)(ModListHead32->Flink);//DbgPrint("Module32=%x\n",Module32);
			while (ModListHead32 != Module32)
			{
				if (((PLDR_DATA_TABLE_ENTRY32)Module32)->DllBase)
				{
					memcpy(ModBuffer[count].FullDllName,
						((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Buffer,
						((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Length);
					ModBuffer[count].DllBase = ((PLDR_DATA_TABLE_ENTRY)Module)->DllBase;
				}
				count++;
				Module32 = (PLIST_ENTRY32)(Module32->Flink);
				ProbeForRead((CONST PVOID)Module32, 40, 4);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DbgPrint("[EnumModule32]__except (EXCEPTION_EXECUTE_HANDLER)");
		}
	}
#endif
	KeUnstackDetachProcess(&ks);
	ObDereferenceObject(eprocess);
}
int nf_DumpProcess(PKERNEL_COPY_MEMORY_OPERATION request)
{
	PEPROCESS targetProcess;

	if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)request->targetProcessId, &targetProcess)))
	{
		SIZE_T readBytes = 0;
		MmCopyVirtualMemory(targetProcess, request->bufferAddress, PsGetCurrentProcess(), request->targetAddress, request->bufferSize, UserMode, &readBytes);
		ObDereferenceObject(targetProcess);
	}
	return 1;
}
NTSTATUS nf_KillProcess(ULONG hProcessId)
{
	PVOID pPspTerminateThreadByPointerAddress = NULL;
	PEPROCESS pEProcess = NULL;
	PETHREAD pEThread = NULL;
	PEPROCESS pThreadEProcess = NULL;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG i = 0;
#ifdef _WIN64
	// 64 λ
	typedef NTSTATUS(__fastcall* PSPTERMINATETHREADBYPOINTER) (PETHREAD pEThread, NTSTATUS ntExitCode, BOOLEAN bDirectTerminate);
#else
	// 32 λ
	typedef NTSTATUS(*PSPTERMINATETHREADBYPOINTER) (PETHREAD pEThread, NTSTATUS ntExitCode, BOOLEAN bDirectTerminate);
#endif
	// ��ȡ PspTerminateThreadByPointer ������ַ
	pPspTerminateThreadByPointerAddress = GetPspLoadImageNotifyRoutine();
	if (FALSE == MmIsAddressValid(pPspTerminateThreadByPointerAddress))
		return status;
	status = PsLookupProcessByProcessId((HANDLE)hProcessId, &pEProcess);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	for (i = 4; i < 0x80000; i = i + 4)
	{
		status = PsLookupThreadByThreadId((HANDLE)i, &pEThread);
		if (NT_SUCCESS(status))
		{
			pThreadEProcess = PsGetThreadProcess(pEThread);
			if (pEProcess == pThreadEProcess)
			{
				((PSPTERMINATETHREADBYPOINTER)pPspTerminateThreadByPointerAddress)(pEThread, 0, 1);
			}
			ObDereferenceObject(pEThread);
		}
	}
	ObDereferenceObject(pEProcess);
	return status;
}

BOOLEAN QueryProcessNamePath1(__in DWORD pid, __out PWCHAR path, __in DWORD pathlen)
{
	BOOLEAN bRet = FALSE;
	CLIENT_ID cid;
	OBJECT_ATTRIBUTES obj;
	HANDLE hProc = NULL;
	NTSTATUS status;

	InitializeObjectAttributes(&obj, NULL, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	cid.UniqueProcess = (HANDLE)pid;
	cid.UniqueThread = NULL;
	status = ZwOpenProcess(&hProc, GENERIC_ALL, &obj, &cid);
	if (NT_SUCCESS(status))
	{
		DWORD dw;
		WCHAR ProcessPath[MAX_PROCESS_PATH_LEN + sizeof(UNICODE_STRING)] = { 0 };
		status = ZwQueryInformationProcess(hProc, ProcessImageFileName, ProcessPath, sizeof(ProcessPath), &dw);
		if (NT_SUCCESS(status))
		{
			PUNICODE_STRING dststring = (PUNICODE_STRING)ProcessPath;
			// 7/29 ���ܻ�����lengthΪ�գ����¿������� - ���޸�
			if ((pathlen > (DWORD)dststring->Length + sizeof(WCHAR)) && dststring->Length)
			{
				RtlMoveMemory(path, dststring->Buffer, dststring->Length + sizeof(WCHAR));
				bRet = TRUE;
			}
		}
		ZwClose(hProc);
	}
	return bRet;
}