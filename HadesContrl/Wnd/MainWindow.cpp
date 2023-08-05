#include "../HpTcpSvc.h"
#include "../Systeminfolib.h"
#include "../Interface.h"
#include "MainWindow.h"

#include <TlHelp32.h>
#include <mutex>
#include <WinUser.h>
#include <UserEnv.h>
#include <stdio.h>
#include <time.h>
#include "../resource.h"
#pragma comment(lib,"Userenv.lib")

const int WM_SHOWTASK = WM_USER + 501;
const int WM_ONCLOSE = WM_USER + 502;
const int WM_ONOPEN = WM_USER + 503;
const int WM_GETMONITORSTATUS = WM_USER + 504;
const int WM_IPS_PROCESS = WM_USER + 600;

// Hades״̬��
static std::mutex			g_hadesStatuscs;
// Start�߳���
static std::mutex			g_startprocesslock;

// ������
static const std::wstring	g_drverName = L"sysmondriver";
static const std::wstring	g_drverNdrName = L"hadesndr";

// Socket
static HpTcpSvc				g_tcpsvc;


bool IsProcessExist(LPCTSTR lpProcessName)
{
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		return false;
	}
	BOOL bResult = Process32First(hProcessSnap, &pe32);
	bool bExist = false;
	string strExeName;
	while (bResult)
	{
		if (lstrcmpi(pe32.szExeFile, lpProcessName) == 0)
		{
			bExist = true;
			break;
		}
		bResult = Process32Next(hProcessSnap, &pe32);
	}
	CloseHandle(hProcessSnap);
	return bExist;
}
std::wstring GetWStringByChar(const char* szString)
{
	std::wstring wstrString = L"";
	try
	{
		if (szString != NULL)
		{
			std::string str(szString);
			wstrString.assign(str.begin(), str.end());
		}
	}
	catch (const std::exception&)
	{
		return wstrString;
	}

	return wstrString;
}

// HpSocket Init
static DWORD WINAPI StartIocpWorkNotify(LPVOID lpThreadParameter)
{
	g_tcpsvc.hpsk_init();
	return 0;
}

// ��������Ƿ�װ
const bool DrvCheckStatus()
{
	std::wstring pszCmd = L"sc start sysmondriver";
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	int nSeriverstatus = SingletonDriverManager::instance()->nf_GetServicesStatus(g_drverName.c_str());
	switch (nSeriverstatus)
	{
		// ��������
	case SERVICE_CONTINUE_PENDING:
	case SERVICE_RUNNING:
	case SERVICE_START_PENDING:
	{
		OutputDebugString(L"Driver Running");
		break;
	}
	break;
	// �Ѱ�װ - δ����
	case SERVICE_STOPPED:
	case SERVICE_STOP_PENDING:
	{
		GetStartupInfo(&si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		// ����������
		PROCESS_INFORMATION pi;
		if (CreateProcess(NULL, (LPWSTR)pszCmd.c_str(), NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		Sleep(3000);
		nSeriverstatus = SingletonDriverManager::instance()->nf_GetServicesStatus(g_drverName.c_str());
		if (SERVICE_RUNNING == nSeriverstatus)
		{
			OutputDebugString(L"sc Driver Running");
			break;
		}
		else
		{
			OutputDebugString(L"sc Driver Install Failuer");
			return false;
		}
	}
	break;
	case 0x424:
	{
		std::string strVerkerLinfo = "";
		bool Is64 = false;
		int verMajorVersion = 0;
		int verMinorVersion = 0;
		SingletonUSysBaseInfo::instance()->GetOSVersion(strVerkerLinfo, verMajorVersion, verMinorVersion, Is64);
		if (!SingletonDriverManager::instance()->nf_DriverInstall_SysMonStart(verMajorVersion, verMinorVersion, Is64))
		{
			MessageBox(NULL, L"������װʧ�ܣ������ֶ���װ�ٴο����ں�̬�ɼ�", L"��ʾ", MB_OKCANCEL);
			return false;
		}
	}
	break;
	default:
		return false;
	}

	return true;
}

const bool NetCheckStatus()
{
	std::wstring pszCmd = L"sc start hadesndr";
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	int nSeriverstatus = SingletonDriverManager::instance()->nf_GetServicesStatus(g_drverNdrName.c_str());
	switch (nSeriverstatus)
	{
		// ��������
	case SERVICE_CONTINUE_PENDING:
	case SERVICE_RUNNING:
	case SERVICE_START_PENDING:
	{
		OutputDebugString(L"Driver Running");
		break;
	}
	break;
	// �Ѱ�װ - δ����
	case SERVICE_STOPPED:
	case SERVICE_STOP_PENDING:
	{
		GetStartupInfo(&si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		// ����������
		PROCESS_INFORMATION pi;
		if (CreateProcess(NULL, (LPWSTR)pszCmd.c_str(), NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		Sleep(3000);
		nSeriverstatus = SingletonDriverManager::instance()->nf_GetServicesStatus(g_drverNdrName.c_str());
		if (SERVICE_RUNNING == nSeriverstatus)
		{
			OutputDebugString(L"sc Driver Running");
			break;
		}
		else
		{
			OutputDebugString(L"sc Driver Install Failuer");
			return false;
		}
	}
	break;
	case 0x424:
	{
		std::string strVerkerLinfo = "";
		bool Is64 = false;
		int verMajorVersion = 0;
		int verMinorVersion = 0;
		SingletonUSysBaseInfo::instance()->GetOSVersion(strVerkerLinfo, verMajorVersion, verMinorVersion, Is64);
		if (!SingletonDriverManager::instance()->nf_DriverInstall_NetMonStart(verMajorVersion, verMinorVersion, Is64))
		{
			MessageBox(NULL, L"����������װʧ�ܣ������ֶ���װ.", L"��ʾ", MB_OKCANCEL);
			return false;
		}
	}
	break;
	default:
		return false;
	}

	return true;
}

// ��������
void killProcess(const wchar_t* const processname)
{

	HANDLE hSnapshort = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshort == INVALID_HANDLE_VALUE)
	{
		return;
	}

	// ����߳��б�  
	PROCESSENTRY32 stcProcessInfo;
	stcProcessInfo.dwSize = sizeof(stcProcessInfo);
	BOOL  bRet = Process32First(hSnapshort, &stcProcessInfo);
	while (bRet)
	{
		if (lstrcatW(stcProcessInfo.szExeFile, processname) == 0)
		{
			HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, stcProcessInfo.th32ProcessID);
			::TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
			break;
		}
		bRet = Process32Next(hSnapshort, &stcProcessInfo);
	}

	CloseHandle(hSnapshort);
}
// ��������
bool StartHadesAgentProcess()
{
	// ����
	wchar_t szModule[4096] = { 0, };
	GetModuleFileName(NULL, szModule, 4096 * sizeof(wchar_t));
	std::wstring dirpath = szModule;
	if (0 >= dirpath.size())
		return false;
	const int offset = dirpath.rfind(L"\\");
	if (0 >= offset)
		return false;
	dirpath = dirpath.substr(0, (offset + 1));

	std::wstring cmdline;
	cmdline += dirpath;

	HANDLE hToken = NULL;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	void* Environ;
	if (!CreateEnvironmentBlock(&Environ, hToken, FALSE))
		Environ = NULL;

	RtlZeroMemory(&si, sizeof(STARTUPINFO));
	RtlZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	si.cb = sizeof(STARTUPINFO);
	si.lpReserved = NULL;
	si.lpDesktop = NULL;
	si.lpTitle = NULL;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.cbReserved2 = NULL;
	si.lpReserved2 = NULL;

	// Start HadesAgent.exe
#ifdef _WIN64
	cmdline += L"HadesAgent64.exe";
#else
	cmdline += L"HadesAgent.exe";
#endif
	BOOL ok = CreateProcess(cmdline.c_str(), NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
	if (Environ)
		DestroyEnvironmentBlock(Environ);
	if (ok) {

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	return ok;
}

// Agent/Svc/���״̬ˢ��
void MainWindow::UpdateHadesSvcStatus()
{
	try
	{
#ifdef _WIN64
		if (!IsProcessExist(L"HadesSvc64.exe"))
#else
		if (!IsProcessExist(L"HadesSvc.exe"))
#endif
		{
			if (!m_hadesSvcStatus)
				return;
			m_pImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectImg")));
			m_pConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectStatus")));
			m_pImage_lab->SetBkImage(L"img/normal/winmain_connectfailuer1.png");
			m_pConnectSvc_lab->SetText(L"HadesSvcδ����");
			g_hadesStatuscs.lock();
			m_hadesSvcStatus = false;
			g_hadesStatuscs.unlock();
			// Set View Button
			::PostMessage(m_hWnd, WM_GETMONITORSTATUS, 0x26, NULL);
		}
		else
		{
			if (m_hadesSvcStatus)
				return;
			m_pImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectImg")));
			m_pConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectStatus")));
			m_pImage_lab->SetBkImage(L"img/normal/winmain_connectsuccess.png");
			m_pConnectSvc_lab->SetText(L"HadesSvc�Ѽ���");
			g_hadesStatuscs.lock();
			m_hadesSvcStatus = true;
			g_hadesStatuscs.unlock();
		}
	}
	catch (const std::exception&)
	{

	}
}
void MainWindow::UpdateHadesAgentStatus()
{
	try
	{
		// ���HadesAgent�����Ƿ����
#ifdef _WIN64
		if (!IsProcessExist(L"HadesAgent64.exe"))
#else
		if (!IsProcessExist(L"HadesAgent.exe"))
#endif
		{
			if (!m_hadesAgentStatus)
				return;
			m_pAgentImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectImg")));
			m_pAgentConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectStatus")));
			m_pAgentImage_lab->SetBkImage(L"img/normal/winmain_connectfailuer1.png");
			m_pAgentConnectSvc_lab->SetText(L"HadesAgentδ����");
			g_hadesStatuscs.lock();
			m_hadesAgentStatus = false;
			g_hadesStatuscs.unlock();
		}
		else
		{
			if (m_hadesAgentStatus)
				return;
			m_pAgentImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectImg")));
			m_pAgentConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectStatus")));
			m_pAgentImage_lab->SetBkImage(L"img/normal/winmain_connectsuccess.png");
			m_pAgentConnectSvc_lab->SetText(L"HadesAgent�Ѽ���");
			g_hadesStatuscs.lock();
			m_hadesAgentStatus = true;
			g_hadesStatuscs.unlock();
		}
	}
	catch (const std::exception&)
	{

	}
}
void MainWindow::UpdateMonitorSvcStatus(LPARAM lParam)
{
	try
	{
		const int dStatusId = (DWORD)lParam;
		if (!dStatusId && (0x20 <= dStatusId) && (0x26 >= dStatusId))
			return;
		// �û�̬���
		static COptionUI* pUOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonUserBtn")));
		static COptionUI* pKOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonKerBtn")));
		static COptionUI* pMOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonBeSnipingBtn")));
		switch (dStatusId)
		{
		case 0x20:
			pUOption->Selected(true);
			break;
		case 0x21:
			pUOption->Selected(false);
			break;
		case 0x22:
			pKOption->Selected(true);
			break;
		case 0x23:
			pKOption->Selected(false);
			break;
		case 0x24:
			pMOption->Selected(true);
			break;
		case 0x25:
			pMOption->Selected(false);
			break;
		case 0x26:
			pUOption->Selected(false);
			pKOption->Selected(false);
			pMOption->Selected(false);
			break;
		default:
			break;
		}
	}
	catch (const std::exception&)
	{

	}
}

// ע:GoAgentû��ʹ��CreateEvent�¼�������Ҳ�����¼��ȴ��Ͷ�ʱ���� - �߳���5s���һ��
// HadesAgent״̬չʾ
void MainWindow::GetHadesAgentStatus()
{
	//���HadesAgent�Ƿ����
	for (;;)
	{
		UpdateHadesAgentStatus();
		Sleep(5000);
	}
}
static DWORD WINAPI HadesAgentActiveEventNotify(LPVOID lpThreadParameter)
{
	(reinterpret_cast<MainWindow*>(lpThreadParameter))->GetHadesAgentStatus();
	return 0;
}
// HadesSvc״̬չʾ
void MainWindow::GetHadesSvcStatus()
{
	// ���HadesSvc�����Ƿ����
	for (;;)
	{
		UpdateHadesSvcStatus();
		Sleep(5000);
	}
}
static DWORD WINAPI HadesSvcActiveEventNotify(LPVOID lpThreadParameter)
{
	(reinterpret_cast<MainWindow*>(lpThreadParameter))->GetHadesSvcStatus();
	return 0;
}
// ���״̬չʾ
void MainWindow::GetMonitorStatus()
{
	// ���HadesSvc����ʹ�õļ�ط���
	for (;;)
	{
		HWND m_SvcHwnd = FindWindow(L"HadesSvc", L"HadesSvc");
		if (m_SvcHwnd)
			::PostMessage(m_SvcHwnd, WM_GETMONITORSTATUS, NULL, NULL);
		Sleep(5000);
	}
}
static DWORD WINAPI HadesMonitorNotify(LPVOID lpThreadParameter)
{
	(reinterpret_cast<MainWindow*>(lpThreadParameter))->GetMonitorStatus();
	return 0;
}

CDuiString MainWindow::GetSkinFile()
{
	return _T("MainWindow.xml");
}
CDuiString MainWindow::GetSkinFolder()
{
	return _T("");
}
LPCTSTR MainWindow::GetWindowClassName() const
{
	return _T("HadesMainWindow");
}

void MainWindow::InitWindows()
{
	try
	{
		//��ʼ������
		Systeminfolib libobj;
		CLabelUI* pCurrentUser_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("mainwin_currentuser_lab")));
		pCurrentUser_lab->SetText(GetWStringByChar(SYSTEMPUBLIC::sysattriinfo.currentUser.c_str()).c_str());
		CLabelUI* pCpu_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("mainwin_cpu_lab")));
		pCpu_lab->SetText(GetWStringByChar(SYSTEMPUBLIC::sysattriinfo.cpuinfo.c_str()).c_str());
		CLabelUI* pSysver_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("mainwin_sysver_lab")));
		pSysver_lab->SetText(GetWStringByChar(SYSTEMPUBLIC::sysattriinfo.verkerlinfo.c_str()).c_str());
		CLabelUI* pMainbocard_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("mainwin_mainbocard_lab")));
		if (!SYSTEMPUBLIC::sysattriinfo.mainboard.empty())
			pMainbocard_lab->SetText(GetWStringByChar(SYSTEMPUBLIC::sysattriinfo.mainboard[0].c_str()).c_str());
		CLabelUI* pBattery_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("mainwin_battery_lab")));
		if (!SYSTEMPUBLIC::sysattriinfo.monitor.empty())
			pBattery_lab->SetText(GetWStringByChar(SYSTEMPUBLIC::sysattriinfo.monitor[0].c_str()).c_str());

		m_pImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectImg")));
		m_pConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerSvcConnectStatus")));
#ifdef _WIN64
		if (!IsProcessExist(L"HadesSvc64.exe"))
#else
		if (!IsProcessExist(L"HadesSvc.exe"))
#endif
		{
			m_pImage_lab->SetBkImage(L"img/normal/winmain_connectfailuer1.png");
			m_pConnectSvc_lab->SetText(L"HadesSvcδ����");
			g_hadesStatuscs.lock();
			m_hadesSvcStatus = false;
			g_hadesStatuscs.unlock();
		}
		else
		{
			m_pImage_lab->SetBkImage(L"img/normal/winmain_connectsuccess.png");
			m_pConnectSvc_lab->SetText(L"HadesSvc�Ѽ���");
			g_hadesStatuscs.lock();
			m_hadesSvcStatus = true;
			g_hadesStatuscs.unlock();
		}

		m_pAgentImage_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectImg")));
		m_pAgentConnectSvc_lab = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("ServerAgentConnectStatus")));
#ifdef _WIN64
		if (!IsProcessExist(L"HadesAgent64.exe"))
#else
		if (!IsProcessExist(L"HadesAgent.exe"))
#endif
		{
			m_pAgentImage_lab->SetBkImage(L"img/normal/winmain_connectfailuer1.png");
			m_pAgentConnectSvc_lab->SetText(L"HadesAgentδ����");
			g_hadesStatuscs.lock();
			m_hadesAgentStatus = false;
			g_hadesStatuscs.unlock();
		}
		else
		{
			m_pAgentImage_lab->SetBkImage(L"img/normal/winmain_connectsuccess.png");
			m_pAgentConnectSvc_lab->SetText(L"HadesAgent�Ѽ���");
			g_hadesStatuscs.lock();
			m_hadesAgentStatus = true;
			g_hadesStatuscs.unlock();
		}

		pMainOptemp = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("MainOptemperature_VLayout")));
		pMainOpcpu = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("MainOpCpu_VLayout")));
		pMainOpbox = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("MainOpBox_VLayout")));
		pMainOptemp->SetVisible(false);
		pMainOpbox->SetVisible(false);
		pMainOpcpu->SetVisible(true);
	}
	catch (const std::exception&)
	{

	}
}
void MainWindow::AddTrayIcon() {
	memset(&m_trayInfo, 0, sizeof(NOTIFYICONDATA));
	m_trayInfo.cbSize = sizeof(NOTIFYICONDATA);
	m_trayInfo.hIcon = ::LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_HADESCONTRL));
	m_trayInfo.hWnd = m_hWnd;
	lstrcpy(m_trayInfo.szTip, _T("Hades"));
	m_trayInfo.uCallbackMessage = WM_SHOWTASK;
	m_trayInfo.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	Shell_NotifyIcon(NIM_ADD, &m_trayInfo);
	ShowWindow(SW_HIDE);
}
LRESULT MainWindow::OnTrayIcon(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (lParam == WM_LBUTTONDOWN)
	{
		Shell_NotifyIcon(NIM_DELETE, &m_trayInfo);
		::ShowWindow(m_hWnd, SW_SHOWNORMAL);
	}
	if (lParam == WM_RBUTTONDOWN)
	{
		POINT pt; 
		GetCursorPos(&pt);
		SetForegroundWindow(m_hWnd);
		HMENU hMenu;
		hMenu = CreatePopupMenu();
		AppendMenu(hMenu, MF_STRING, WM_ONCLOSE, _T("�˳�"));
		AppendMenu(hMenu, MF_STRING, WM_ONOPEN, _T("��������"));
		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, NULL, m_hWnd, NULL);
		if (cmd == WM_ONCLOSE)
		{
			Shell_NotifyIcon(NIM_DELETE, &m_trayInfo);
			m_trayInfo.hIcon = NULL;	
			::PostQuitMessage(0);
		}
		else if (cmd == WM_ONOPEN)
		{
			::ShowWindow(m_hWnd, SW_SHOW);
			Shell_NotifyIcon(NIM_DELETE, &m_trayInfo);
		}
	}
	bHandled = true;
	return 0;
}
LRESULT MainWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRes = __super::OnCreate(uMsg, wParam, lParam, bHandled);

	// Create Meue
	m_pMenu = new Menu();
	m_pMenu->Create(m_hWnd, _T(""), WS_POPUP, WS_EX_TOOLWINDOW);
	m_pMenu->ShowWindow(false);

	// ��ʼ����������
	InitWindows();
	
	// ���HadesAgent����
	CreateThread(NULL, NULL, HadesAgentActiveEventNotify, this, 0, 0);
	Sleep(100);

	// ���HadesSvc����
	CreateThread(NULL, NULL, HadesSvcActiveEventNotify, this, 0, 0);
	Sleep(100);

	// �����״̬
	CreateThread(NULL, NULL, HadesMonitorNotify, this, 0, 0);
	Sleep(100);
	
	// ���ö�ʱ��,ˢ�½�������(cpu,mem)
	SetTimer(m_hWnd, 1, 1000, NULL);
	
	// ����HpSocketServer�ȴ�HadesSvc - HpSocket������Ϊ���ؽ���
	CreateThread(NULL, NULL, StartIocpWorkNotify, this, 0, 0);
	return lRes;
}
LRESULT MainWindow::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	KillTimer(m_hWnd, 1);
	Sleep(100);
	// �����˳��Ƿ�HadesSvc�˳�?
	//const auto exithandSvc = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"Global\\HadesSvc_EVNET_EXIT");
	//if (exithandSvc)
	//{
	//	SetEvent(exithandSvc);
	//	CloseHandle(exithandSvc);
	//}
	
	// �˳�HpSocket
	auto IocpExEvt = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"HpStopTcpSvcEvent");
	if (IocpExEvt)
	{
		SetEvent(IocpExEvt);
		Sleep(100);
		CloseHandle(IocpExEvt);
	}
	return __super::OnClose(uMsg, wParam, lParam, bHandled);
}

void MainWindow::FlushData()
{
	try
	{
		//cpu
		const double cpuutilize = SingletonUSysBaseInfo::instance()->GetSysDynCpuUtiliza();
		CString m_Cpusyl;
		m_Cpusyl.Format(L"CPU: %0.2lf", cpuutilize);
		m_Cpusyl += "%";
		CLabelUI* pCpuut = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("winmain_layout_cpuinfo")));
		if (pCpuut)
			pCpuut->SetText(m_Cpusyl.GetBuffer());

		//memory
		const DWORD dwMem = SingletonUSysBaseInfo::instance()->GetSysDynSysMem();
		// ��ǰռ���� Occupancy rate
		CString m_MemoryBFB;
		m_MemoryBFB.Format(L"�ڴ�: %u", dwMem);
		m_MemoryBFB += "%";
		CLabelUI* pMem = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("winmain_layout_memory")));
		if (pMem)
			pMem->SetText(m_MemoryBFB.GetBuffer());
	}
	catch (const std::exception&)
	{

	}
}
static DWORD WINAPI ThreadFlush(LPVOID lpThreadParameter)
{
	if (lpThreadParameter)
		(reinterpret_cast<MainWindow*>(lpThreadParameter))->FlushData();
	return 0;
}
LRESULT MainWindow::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	QueueUserWorkItem(ThreadFlush, this, WT_EXECUTEDEFAULT);
	bHandled = false;
	return 0;
}

void MainWindow::Notify(TNotifyUI& msg)
{
	CDuiString strClassName = msg.pSender->GetClass();
	CDuiString strControlName = msg.pSender->GetName();

	if (msg.sType == DUI_MSGTYPE_WINDOWINIT);
	else if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		if (strClassName == DUI_CTR_BUTTON)
		{
			if (strControlName == _T("MainCloseBtn"))
			{
				const int nret = MessageBox(m_hWnd, L"����ر�,��ϣ���Ƿ����������̣�", L"��ʾ", MB_OKCANCEL | MB_ICONWARNING);
				if (1 == nret)
					AddTrayIcon();
				else
					Close();
			}
			else if (strControlName == _T("MainMenuBtn"))
			{//�˵�
				int xPos = msg.pSender->GetPos().left - 36;
				int yPos = msg.pSender->GetPos().bottom;
				POINT pt = { xPos, yPos };
				ClientToScreen(m_hWnd, &pt);
				m_pMenu->ShowWindow(true);
				::SetWindowPos(m_pMenu->GetHWND(), NULL, pt.x, pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			//��С��
			else if (strControlName == _T("MainMinsizeBtn"))
			{
				::ShowWindow(m_hWnd, SW_MINIMIZE);
			}
			else if (strControlName == _T("StartHadesAgentExe"))
			{
#ifdef _WIN64
				if (!IsProcessExist(L"HadesAgent64.exe"))
#else
				if (!IsProcessExist(L"HadesAgent.exe"))
#endif
				{
					if(StartHadesAgentProcess())
						MessageBox(m_hWnd, L"�ɹ�����HadesAgent�ɹ�", L"��ʾ", MB_OK);
					else
						MessageBox(m_hWnd, L"��������HadesAgentʧ��,����ϵ����Ա", L"��ʾ", MB_OK);
				}
				else
				{
					MessageBox(m_hWnd, L"HadesAgent������������������ϵ�Ų�", L"��ʾ", MB_OK);
				}
			}
		}
		else if (strClassName == DUI_CTR_OPTION)
		{
			if (_tcscmp(static_cast<COptionUI*>(msg.pSender)->GetGroup(), _T("MainOpView")) == 0)
			{

				if (strControlName == _T("MainMontemperatureOpt"))
				{
					// MainOptemperature_VLayout
					pMainOptemp->SetVisible(true);
					pMainOpcpu->SetVisible(false);
					pMainOpbox->SetVisible(false);
				}
				else if (strControlName == _T("MainMonCpuOpt"))
				{
					// MainOpCpu_VLayout
					pMainOptemp->SetVisible(false);
					pMainOpcpu->SetVisible(true);
					pMainOpbox->SetVisible(false);
				}
				else if (strControlName == _T("MainMonBoxOpt"))
				{
					// MainOpBox_VLayout
					pMainOptemp->SetVisible(false);
					pMainOpcpu->SetVisible(false);
					pMainOpbox->SetVisible(true);
				}
			}
			else if (strControlName == _T("MainMonUserBtn"))
			{//�·��û�̬���ָ��
				//COptionUI* pOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonUserBtn")));
				//if (!pOption)
				//	return;
				//if (false == m_hadesSvcStatus)
				//{
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"��������Grpc�ϱ�ƽ̨�������ɼ�", L"��ʾ", MB_OK);
				//	return;
				//}
				//HWND m_SvcHwnd = FindWindow(L"HadesSvc", L"HadesSvc");
				//COPYDATASTRUCT c2_;
				//c2_.dwData = 1;
				//c2_.cbData = 0;
				//c2_.lpData = NULL;
				////������Ϣ
				//::SendMessage(m_SvcHwnd, WM_COPYDATA, NULL, (LPARAM)&c2_);
			}
			else if (strControlName == _T("MainMonKerBtn"))
			{//�·��ں�̬���ָ��
				//COptionUI* pOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonKerBtn")));
				//if (!pOption)
				//	return;
				//if (false == m_hadesSvcStatus)
				//{
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"��������Grpc�ϱ�ƽ̨�������ɼ�", L"��ʾ", MB_OK);
				//	return;
				//}
				//if (SYSTEMPUBLIC::sysattriinfo.verMajorVersion < 6)
				//{
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"��ǰϵͳ����ģʽ�����ݣ��뱣֤����ϵͳwin7~win10֮��", L"��ʾ", MB_OK);
				//	return;
				//}
				//const bool nret = DrvCheckStart();
				//if (true == nret)
				//{
				//	HWND m_SvcHwnd = FindWindow(L"HadesSvc", L"HadesSvc");
				//	COPYDATASTRUCT c2_;
				//	c2_.dwData = 2;
				//	c2_.cbData = 0;
				//	c2_.lpData = NULL;
				//	::SendMessage(m_SvcHwnd, WM_COPYDATA, NULL, (LPARAM)&c2_);
				//}
				//else {
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"�ں�̬�������ʧ��\n��ʹ��cmd: sc query/delete hadesmondrv�鿴����״̬\ndeleteɾ���������¿�����", L"��ʾ", MB_OK);
				//}
			}
			else if (strControlName == _T("MainMonBeSnipingBtn"))
			{//���ض�����Ϊ
				//COptionUI* pOption = static_cast<COptionUI*>(m_PaintManager.FindControl(_T("MainMonBeSnipingBtn")));
				//if (!pOption)
				//	return;
				//if (false == m_hadesSvcStatus)
				//{
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"��������Grpc�ϱ�ƽ̨�������ɼ�", L"��ʾ", MB_OK);
				//	return;
				//}
				//if (SYSTEMPUBLIC::sysattriinfo.verMajorVersion < 6)
				//{
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"��ǰϵͳ����ģʽ�����ݣ��뱣֤����ϵͳwin7~win10֮��", L"��ʾ", MB_OK);
				//	return;
				//}
				//const bool nret = DrvCheckStart();
				//if (true == nret)
				//{
				//	HWND m_SvcHwnd = FindWindow(L"HadesSvc", L"HadesSvc");
				//	COPYDATASTRUCT c2_;
				//	c2_.dwData = 3;
				//	c2_.cbData = 0;
				//	c2_.lpData = NULL;
				//	::SendMessage(m_SvcHwnd, WM_COPYDATA, NULL, (LPARAM)&c2_);
				//}
				//else {
				//	pOption->Selected(true);
				//	MessageBox(m_hWnd, L"�ں�̬�������ʧ��\n��ʹ��cmd: sc query/delete hadesmondrv�鿴����״̬\ndeleteɾ���������¿�����", L"��ʾ", MB_OK);
				//}
			}
		}
	}
}
LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lRes = 0;
	BOOL bHandled = TRUE;
	return __super::HandleMessage(uMsg, wParam, lParam);
}
LRESULT MainWindow::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRes = 0;
	bHandled = TRUE;

	switch (uMsg) {
	case WM_TIMER: lRes = OnTimer(uMsg, wParam, lParam, bHandled); break;	// ˢ�½�������
	case WM_SHOWTASK: OnTrayIcon(uMsg, wParam, lParam, bHandled); break;	// ���̴���
	case WM_GETMONITORSTATUS: UpdateMonitorSvcStatus(wParam); break;		// ������״̬
	case WM_IPS_PROCESS: break;
	default:
		bHandled = FALSE;
		break;
	}
	if (bHandled) return lRes;
	return __super::HandleCustomMessage(uMsg, wParam, lParam, bHandled);
}