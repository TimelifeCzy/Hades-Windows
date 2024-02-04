#include <Windows.h>
#include <xstring>
#include "udrivermanager.h"

DriverManager::DriverManager()
{
}

DriverManager::~DriverManager()
{
}

#define				SECURITY_STRING_LEN							168
#define				LG_PAGE_SIZE								4096
#define				MAX_KEY_LENGTH								1024
#define				LG_SLEEP_TIME								4000

// Wfp��װ��ʽ
bool SplitFilePath(const char* szFullPath, char* szPath, char* szFileName, char* szFileExt)
{
	char* p = nullptr, * q = nullptr, * r = nullptr;
	size_t	len = 0;

	if (NULL == szFullPath)
	{
		return false;
	}
	p = (char*)szFullPath;
	len = strlen(szFullPath);
	if (szPath)
	{
		szPath[0] = 0;
	}
	if (szFileName)
	{
		szFileName[0] = 0;
	}
	if (szFileExt)
	{
		szFileExt[0] = 0;
	}
	q = p + len;
	while (q > p)
	{
		if (*q == '\\' || *q == '/')
		{
			break;
		}
		q--;
	}
	if (q <= p)
	{
		return false;
	}
	if (szPath)
	{
		memcpy(szPath, p, q - p + 1);
		szPath[q - p + 1] = 0;
	}
	q++;
	p = q;
	r = NULL;
	while (*q)
	{
		if (*q == '.')
		{
			r = q;
		}
		q++;
	}
	if (NULL == r)
	{
		if (szFileName)
		{
			memcpy(szFileName, p, q - p + 1);
		}
	}
	else
	{
		if (szFileName)
		{
			memcpy(szFileName, p, r - p);
			szFileName[r - p] = 0;
		}
		if (szFileExt)
		{
			memcpy(szFileExt, r + 1, q - r + 1);
		}
	}

	return true;
}
int FindInMultiSz(LPTSTR szMultiSz, int nMultiSzLen, LPTSTR szMatch)
{
	size_t	i, j;
	size_t	len = lstrlenW(szMatch);
	TCHAR	FirstChar = *szMatch;
	bool	bFound;
	LPTSTR	pTry;

	if (NULL == szMultiSz || NULL == szMatch || nMultiSzLen <= 0)
	{
		return -1;
	}
	for (i = 0; i < nMultiSzLen - len; i++)
	{
		if (*szMultiSz++ == FirstChar)
		{
			bFound = true;
			pTry = szMultiSz;
			for (j = 1; j <= len; j++)
			{
				if (*pTry++ != szMatch[j])
				{
					bFound = false;
					break;
				}
			}
			if (bFound)
			{
				return (int)i;
			}
		}
	}

	return -1;
}
int CreateDriver(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	SC_HANDLE		schManager;
	SC_HANDLE		schService;
	SERVICE_STATUS	svcStatus;
	bool			bStopped = false;
	int				i;

	if (NULL == cszDriverName || NULL == cszDriverFullPath)
	{
		return -1;
	}
	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return -1;
	}
	schService = OpenService(schManager, cszDriverName, SERVICE_ALL_ACCESS);
	if (NULL != schService)
	{
		if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus))
		{
			if (svcStatus.dwCurrentState != SERVICE_STOPPED)
			{
				if (0 == ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus))
				{
					CloseServiceHandle(schService);
					CloseServiceHandle(schManager);
					return -1;
				}
				for (i = 0; i < 10; i++)
				{
					if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus) == 0 || svcStatus.dwCurrentState == SERVICE_STOPPED)
					{
						bStopped = true;
						break;
					}
					Sleep(LG_SLEEP_TIME);
				}
				if (!bStopped)
				{
					CloseServiceHandle(schService);
					CloseServiceHandle(schManager);
					return -1;
				}
			}
		}
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return 0;
	}
	schService = CreateService(schManager, cszDriverName, cszDriverName, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, cszDriverFullPath, NULL, NULL, NULL, NULL, NULL);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return -1;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return 0;
}
int StartDriver(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	SC_HANDLE		schManager;
	SC_HANDLE		schService;
	SERVICE_STATUS	svcStatus;
	bool			bStarted = false;
	int				i;

	if (NULL == cszDriverName)
	{
		return -1;
	}
	if (CreateDriver(cszDriverName, cszDriverFullPath) < 0)
	{
		return -1;
	}
	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return -1;
	}
	schService = OpenService(schManager, cszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return -1;
	}
	if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus))
	{
		if (svcStatus.dwCurrentState == SERVICE_RUNNING)
		{
			CloseServiceHandle(schService);
			CloseServiceHandle(schManager);
			return 0;
		}
	}
	else if (GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return -1;
	}
	if (0 == StartService(schService, 0, NULL))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return -1;
	}
	for (i = 0; i < 10; i++)
	{
		if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus) && svcStatus.dwCurrentState == SERVICE_RUNNING)
		{
			bStarted = true;
			break;
		}
		Sleep(LG_SLEEP_TIME);
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return bStarted ? 1 : -1;
}
int StopDriver(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	SC_HANDLE		schManager;
	SC_HANDLE		schService;
	SERVICE_STATUS	svcStatus;
	bool			bStopped = false;
	int				i;

	schManager = OpenSCManager(NULL, 0, 0);
	if (NULL == schManager)
	{
		return -1;
	}
	schService = OpenService(schManager, cszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return -1;
	}
	if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus))
	{
		if (svcStatus.dwCurrentState != SERVICE_STOPPED)
		{
			if (0 == ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus))
			{
				CloseServiceHandle(schService);
				CloseServiceHandle(schManager);
				return -1;
			}
			for (i = 0; i < 10; i++)
			{
				if (ControlService(schService, SERVICE_CONTROL_INTERROGATE, &svcStatus) == 0 || svcStatus.dwCurrentState == SERVICE_STOPPED)
				{
					bStopped = true;
					break;
				}
				Sleep(LG_SLEEP_TIME);
			}
			if (!bStopped)
			{
				CloseServiceHandle(schService);
				CloseServiceHandle(schManager);
				return -1;
			}
		}
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return 0;
}

// Minfilter�߶Ȱ�װ��ʽ
const bool InstallDriver1(const wchar_t* cszDriverName, const wchar_t* lpszDriverPath, const wchar_t* lpszAltitude)
{
	HKEY		hKey;
	DWORD		dwData = 0;
	wchar_t		szTempStr[MAX_PATH] = { 0, };
	wchar_t		szDriverImagePath[MAX_PATH] = { 0, };

	if (NULL == cszDriverName || NULL == lpszDriverPath)
	{
		return FALSE;
	}
	GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);
	SC_HANDLE hServiceMgr = NULL;
	SC_HANDLE hService = NULL;

	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!hServiceMgr || (hServiceMgr == NULL))
	{
		//CloseServiceHandle(hServiceMgr);
		return FALSE;
	}
	hService = CreateService(hServiceMgr,
		cszDriverName,					// �����������ע����е�����
		cszDriverName,					// ע������������DisplayName ֵ
		SERVICE_ALL_ACCESS,				// ������������ķ���Ȩ��
		SERVICE_FILE_SYSTEM_DRIVER,		// ��ʾ���صķ������ļ�ϵͳ��������
		SERVICE_DEMAND_START,			// ע������������Start ֵ
		SERVICE_ERROR_IGNORE,			// ע������������ErrorControl ֵ
		lpszDriverPath,					// ע������������ImagePath ֵ
		L"FSFilter Activity Monitor",	// ע������������Group ֵ
		NULL,
		L"FltMgr",						// ע������������DependOnService ֵ
		NULL,
		NULL);

	if (hService == NULL)
	{
		CloseServiceHandle(hServiceMgr);
		if (GetLastError() == ERROR_SERVICE_EXISTS)
			return TRUE;
		else
			return FALSE;
	}
	CloseServiceHandle(hService);
	CloseServiceHandle(hServiceMgr);

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances�ӽ��µļ�ֵ�� 
	//-------------------------------------------------------------------------------------------------------
	lstrcpyW(szTempStr, L"SYSTEM\\CurrentControlSet\\Services\\");
	lstrcatW(szTempStr, cszDriverName);
	lstrcatW(szTempStr, L"\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	lstrcpyW(szTempStr, cszDriverName);
	lstrcatW(szTempStr, L" Instance");
	if (RegSetValueEx(hKey, L"DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlenW(szTempStr) * 2) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);
	RegCloseKey(hKey);


	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance�ӽ��µļ�ֵ�� 
	//-------------------------------------------------------------------------------------------------------
	lstrcpyW(szTempStr, L"SYSTEM\\CurrentControlSet\\Services\\");
	lstrcatW(szTempStr, cszDriverName);
	lstrcatW(szTempStr, L"\\Instances\\");
	lstrcatW(szTempStr, cszDriverName);
	lstrcatW(szTempStr, L" Instance");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// Minifilter��Ҫ��ע������������Altitude ֵ
	lstrcpyW(szTempStr, lpszAltitude);
	if (RegSetValueEx(hKey, L"Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlenW(szTempStr) * 2) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	dwData = 0x0;
	if (RegSetValueEx(hKey, L"Flags", 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);
	RegCloseKey(hKey);

	return TRUE;
}
const BOOL StartDriver1(const wchar_t* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;

	if (NULL == lpszDriverName)
	{
		return FALSE;
	}

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!schManager || (NULL == schManager))
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (!schService || (NULL == schService))
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}

	if (!StartService(schService, 0, NULL))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
		{
			// �����Ѿ�����
			return TRUE;
		}
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}
const BOOL StopDriver1(const wchar_t* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;
	SERVICE_STATUS    svcStatus;
	bool            bStopped = false;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}
const int GetServicesStatus(const wchar_t* driverName)
{
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwOldCheckPoint = 0;
	DWORD dwStartTickCount = 0;
	DWORD dwWaitTime = 0;
	DWORD dwBytesNeeded = 0;

	schSCManager = OpenSCManager(
		NULL,                                // local computer
		NULL,                                // ServicesActive database
		SC_MANAGER_ALL_ACCESS);              // full access rights

	if (NULL == schSCManager)
	{
		OutputDebugStringA("hades OpenSCManager error");
		return GetLastError();

	}

	schService = OpenService(
		schSCManager,                      // SCM database
		driverName,                         // name of service
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS);     // full access

	if (schService == NULL)
	{
		
		OutputDebugStringA("OpenService failed");
		CloseServiceHandle(schSCManager);
		return GetLastError();
	}

	if (!QueryServiceStatusEx(
		schService,                         // handle to service
		SC_STATUS_PROCESS_INFO,             // information level
		(LPBYTE)&ssStatus,                 // address of structure
		sizeof(SERVICE_STATUS_PROCESS),     // size of structure
		&dwBytesNeeded))                  // size needed if buffer is too small
	{
		OutputDebugStringA("QueryServiceStatusEx failed");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return -1;
	}
	return ssStatus.dwCurrentState;
}
const int DeleteDriver(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	// ж��
	SC_HANDLE    schManager;
	SC_HANDLE    schService;
	SERVICE_STATUS  svcStatus;

	schManager = OpenSCManager(NULL, 0, 0);
	if (NULL == schManager)
	{
		return -1;
	}
	schService = OpenService(schManager, cszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return -1;
	}
	ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
	if (0 == DeleteService(schService))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return -1;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	// ɾ���ļ�
	DeleteFile(cszDriverFullPath);
	return 0;
}
const LONG RegDeleteKeyNT(HKEY hStartKey, LPTSTR pKeyName)
{
	DWORD  dwSubKeyLength;
	LPTSTR  pSubKey = NULL;
	TCHAR  szSubKey[MAX_KEY_LENGTH];
	HKEY  hKey;
	LONG  lRet;

	if (pKeyName && lstrlenW(pKeyName))
	{
		if ((lRet = RegOpenKeyEx(hStartKey, pKeyName, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &hKey)) == ERROR_SUCCESS)
		{
			while (lRet == ERROR_SUCCESS)
			{
				dwSubKeyLength = MAX_KEY_LENGTH;
				lRet = RegEnumKeyEx(hKey, 0, szSubKey, (LPDWORD)&dwSubKeyLength, NULL, NULL, NULL, NULL);
				if (lRet == ERROR_NO_MORE_ITEMS)
				{
					lRet = RegDeleteKey(hStartKey, pKeyName);
					break;
				}
				else if (lRet == ERROR_SUCCESS)
				{
					lRet = RegDeleteKeyNT(hKey, szSubKey);
				}
			}
			RegCloseKey(hKey);
		}
	}
	else
	{
		lRet = ERROR_BADKEY;
	}

	return lRet;
}
const int RemoveDriver(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	HKEY hKey;
	long errorno;
	char szBuf[LG_PAGE_SIZE];
	wchar_t szDriverName[MAX_PATH];

	memset(szBuf, 0, LG_PAGE_SIZE);
	memset(szDriverName, 0, MAX_PATH);
	lstrcpyW(szDriverName, cszDriverName);
	strcpy_s(szBuf, "SYSTEM//CurrentControlSet//Services//");
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, szBuf, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
	{
		return -1;
	}
	if ((errorno = RegDeleteKeyNT(hKey, szDriverName)) != ERROR_SUCCESS)
	{
		return -1;
	}
	RegCloseKey(hKey);
	return 0;
}

const int DriverManager::nf_GetServicesStatus(const wchar_t* driverName)
{
	return GetServicesStatus(driverName);
}

const int DriverManager::nf_StartDrv(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	return StartDriver(cszDriverName, cszDriverFullPath);
}

const int DriverManager::nf_StopDrv(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	return StopDriver(cszDriverName, cszDriverFullPath);
}

const int DriverManager::nf_DeleteDrv(const wchar_t* cszDriverName, const wchar_t* cszDriverFullPath)
{
	return DeleteDriver(cszDriverName, cszDriverFullPath);
}

const bool DriverManager::nf_DriverInstall_SysMonStart(const int mav, const int miv, const bool Is64)
{
	if (mav < 6)
		return false;
	std::wstring DriverPath;
	std::wstring PathAll;
	std::wstring pszCmd = L"sc start sysmondriver";
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	DWORD nSeriverstatus = -1;
	TCHAR szFilePath[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, szFilePath, MAX_PATH);
	OutputDebugString(szFilePath);
	DriverPath = szFilePath;
	const size_t num = DriverPath.find_last_of(L"\\");
	PathAll = DriverPath.substr(0, num);

	// ������׼
	switch (miv)
	{
	case 0://Vista_Server08
	case 1://Win7 or win8
	{
		if(Is64)
			PathAll += L"\\driver\\win7\\64\\sysmondriver.sys";
		else
			PathAll += L"\\driver\\win7\\sysmondriver.sys";
	}
	break;
	case 2://win10
	case 3://win11
	{
		if (Is64)
			PathAll += L"\\driver\\win10\\64\\sysmondriver.sys";
		else
			PathAll += L"\\driver\\win10\\sysmondriver.sys";
	}
	break;
	default:
		return false;
	}
	
	OutputDebugStringW(PathAll.c_str());

	// ��װע���
	if (!InstallDriver1(L"sysmondriver", PathAll.c_str(), L"370020"))
	{
		OutputDebugString(L"InitRegister Driver failuer.");
		return false;
	}
	//if (StartDriver(L"hadesmondrv", PathAll.c_str()) == TRUE)
	if (StartDriver1(L"sysmondriver") == TRUE)
	{
		OutputDebugString(L"Start Driver success.");
		return true;
	}
	else
	{
		OutputDebugString(L"Start Driver failuer.");
		return false;
	}
}

const bool DriverManager::nf_DriverInstall_NetMonStart(const int mav, const int miv, const bool Is64)
{
	if (mav < 6)
		return false;
	std::wstring DriverPath;
	std::wstring PathAll;
	std::wstring pszCmd = L"sc start hadesndr";
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	DWORD nSeriverstatus = -1;
	TCHAR szFilePath[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, szFilePath, MAX_PATH);
	OutputDebugString(szFilePath);
	DriverPath = szFilePath;
	const size_t num = DriverPath.find_last_of(L"\\");
	PathAll = DriverPath.substr(0, num);

	// ������׼
	switch (miv)
	{
	case 0://Vista_Server08
	case 1://Win7 or win8
	{
		if (Is64)
			PathAll += L"\\driver\\win7\\64\\hadesndr.sys";
		else
			PathAll += L"\\driver\\win7\\hadesndr.sys";
	}
	break;
	case 2://win10
	case 3://win11
	{
		if (Is64)
			PathAll += L"\\driver\\win10\\64\\hadesndr.sys";
		else
			PathAll += L"\\driver\\win10\\hadesndr.sys";
	}
	break;
	default:
		return false;
	}

	OutputDebugStringW(PathAll.c_str());

	if (StartDriver(L"hadesndr", PathAll.c_str()) == TRUE)
	{
		OutputDebugString(L"Start Driver hadesndr Success.");
		return true;
	}
	else
	{
		OutputDebugString(L"Start Driver hadesndr Failuer.");
		return false;
	}
}

