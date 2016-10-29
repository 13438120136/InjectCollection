// SetThreadContext.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>

#include "Define.h"

using namespace std;

typedef
NTSTATUS(NTAPI * pfnRtlAdjustPrivilege)(
	UINT32 Privilege,
	BOOLEAN Enable,
	BOOLEAN Client,
	PBOOLEAN WasEnabled);

typedef
NTSTATUS(NTAPI * pfnZwQuerySystemInformation)(
	IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
	OUT PVOID SystemInformation,
	IN UINT32 SystemInformationLength,
	OUT PUINT32 ReturnLength OPTIONAL);

typedef
NTSTATUS(NTAPI *pfnNtQueryInformationProcess)(
	__in HANDLE ProcessHandle,
	__in PROCESSINFOCLASS ProcessInformationClass,
	__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
	__in UINT32 ProcessInformationLength,
	__out_opt PUINT32 ReturnLength
	);

BOOL GrantPriviledge(IN UINT32 Priviledge);

BOOL GetThreadIdByProcessId(IN UINT32 ProcessId, OUT PUINT32 ThreadId);

BOOL GetLoadLibraryAddressInTargetProcessImportTable(IN UINT32 ProcessId, OUT PUINT_PTR FunctionAddress);

BOOL GetPebByProcessId(IN UINT32 ProcessId, OUT PPEB Peb);

BOOL Inject(IN UINT32 ProcessId, IN UINT32 ThreadId);



/*

ShellCode:
010D9000 60                   pushad
010D9001 9C                   pushfd
010D9002 68 AA BB CC DD       push        0DDCCBBAAh		// �����޸�Ϊdll·��
010D9007 FF 15 DD CC BB AA    call        dword ptr ds:[0AABBCCDDh]		// �����޸�Ϊ Kernel32.dll������� LoadLibrary ��ַ
010D900D 9D                   popfd
010D900E 61                   popad
010D900F FF 25 AA BB CC DD    jmp         dword ptr ds:[0DDCCBBAAh]		// ��ת��ԭ����eip

*/

#ifdef _WIN64
// ���� 64 λ dll��ע��Bug���޸�

/*
0:019> u 0x000002b5d5f80000
000002b5`d5f80000 4883ec28        sub     rsp,28h
000002b5`d5f80004 488d0d20000000  lea     rcx,[000002b5`d5f8002b]
000002b5`d5f8000b ff1512000000    call    qword ptr [000002b5`d5f80023]
000002b5`d5f80011 4883c428        add     rsp,28h
000002b5`d5f80015 ff2500000000    jmp     qword ptr [000002b5`d5f8001b]
*/

UINT8	ShellCode[0x100] = {
	0x48,0x83,0xEC,0x28,	// sub rsp ,28h

	0x48,0x8D,0x0d,			// [+4] lea rcx,
	0x00,0x00,0x00,0x00,	// [+7] DllNameOffset = [+43] - [+4] - 7

	// call ��ƫ�ƣ�����ַ����*��
	0xff,0x15,				// [+11]
	0x00,0x00,0x00,0x00,	// [+13] 

	0x48,0x83,0xc4,0x28,	// [+17] add rsp,28h

	// jmp ��ƫ�ƣ�����ַ����*��
	0xff,0x25,				// [+21]
	0x00,0x00,0x00,0x00,	// [+23] LoadLibraryAddressOffset

	// ���ԭ�ȵ� rip
	0x00,0x00,0x00,0x00,	// [+27]
	0x00,0x00,0x00,0x00,	// [+31]

	// ���� loadlibrary��ַ
	0x00,0x00,0x00,0x00,	// [+35] 
	0x00,0x00,0x00,0x00,	// [+39]

// ���dll����·��
//	0x00,0x00,0x00,0x00,	// [+43]
//	0x00,0x00,0x00,0x00		// [+47]
//	......
};
#else
// ���� 32 λ �����д��Dll���ظ�ע��

/*
0:005> u 0x00ca0000
00000000`00ca0000 60              pusha
00000000`00ca0001 9c              pushfq
00000000`00ca0002 681d00ca00      push    0CA001Dh
00000000`00ca0007 ff151900ca00    call    qword ptr [00000000`01940026]
00000000`00ca000d 9d              popfq
00000000`00ca000e 61              popa
00000000`00ca000f ff251500ca00    jmp     qword ptr [00000000`0194002a]

*/

UINT8	ShellCode[0x100] = {
	0x60,					// [+0] pusha
	0x9c,					// [+1] pushf
	0x68,					// [+2] push
	0x00,0x00,0x00,0x00,	// [+3] ShellCode + 
	0xff,0x15,				// [+7] call	
	0x00,0x00,0x00,0x00,	// [+9] LoadLibrary Addr  Addr
	0x9d,					// [+13] popf
	0x61,					// [+14] popa
	0xff,0x25,				// [+15] jmp
	0x00,0x00,0x00,0x00,	// [+17] jmp  eip

	// eip ��ַ
	0x00,0x00,0x00,0x00,	// [+21]
	// LoadLibrary ��ַ
	0x00,0x00,0x00,0x00,	// [+25] 
	// DllFullPath 
	0x00,0x00,0x00,0x00		// [+29] 
};

#endif

WCHAR		DllFullPath[MAX_PATH] = { 0 };

UINT_PTR	LoadLibraryWAddress = 0;

int main()
{
	BOOL	bOk = FALSE;

	// 1.��Ȩ

	bOk = GrantPriviledge(SE_DEBUG_PRIVILEGE);
	if (bOk == FALSE)
	{
		printf("[-]Grant Priviledge Error\r\n");
		return FALSE;
	}

	// 2.��̬��·��

	GetCurrentDirectoryW(MAX_PATH, DllFullPath);
#ifdef _WIN64
	wcscat_s(DllFullPath, L"\\x64NormalDll.dll");
#else
	wcscat_s(DllFullPath, L"\\x86NormalDll.dll");
#endif


	// 3.����Id
	UINT32	ProcessId = 0;
	printf("Input Process Id\r\n");
	scanf_s("%d", &ProcessId);

	// 4.���Ŀ�����LoadLibrary������е�ַ
	bOk = GetLoadLibraryAddressInTargetProcessImportTable(ProcessId, &LoadLibraryWAddress);
	if (bOk == FALSE)
	{
		return 0;
	}

	// 5.����߳�Id
	UINT32	ThreadId = 0;
	bOk = GetThreadIdByProcessId(ProcessId, &ThreadId);

	// 6.ע��
	Inject(ProcessId, ThreadId);


	return 0;
}

/************************************************************************
*  Name : Inject
*  Param: ProcessId			����Id	��IN��
*  Param: ThreadId			�߳�Id	��OUT��
*  Ret  : BOOL
*  ͨ��SuspendThread/GetThreadContext/�޸�ip/SetThreadContext/ResumeThreadContext���ע�빤��
************************************************************************/

BOOL Inject(IN UINT32 ProcessId, IN UINT32 ThreadId)
{
	BOOL		bOk = FALSE;
	CONTEXT		ThreadContext = { 0 };
	PVOID		BufferData = NULL;

	HANDLE		ThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, ThreadId);
	HANDLE		ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);


	// ���ȹ����߳�
	SuspendThread(ThreadHandle);


	ThreadContext.ContextFlags = CONTEXT_ALL;
	if (GetThreadContext(ThreadHandle, &ThreadContext) == FALSE)
	{
		CloseHandle(ThreadHandle);
		CloseHandle(ProcessHandle);
		return FALSE;
	}

	BufferData = VirtualAllocEx(ProcessHandle, NULL, sizeof(ShellCode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (BufferData != NULL)
	{

		if (LoadLibraryWAddress != NULL)
		{
#ifdef _WIN64

			// ShellCode + 43�� �������·��
			PUINT8	v1 = ShellCode + 43;
			memcpy(v1, DllFullPath, (wcslen(DllFullPath) + 1) * sizeof(WCHAR));
			UINT32	DllNameOffset = (UINT32)(((PUINT8)BufferData + 43) - ((PUINT8)BufferData + 4) - 7);
			*(PUINT32)(ShellCode + 7) = DllNameOffset;

			// ShellCode + 35�� ���� LoadLibrary ������ַ
			*(PUINT64)(ShellCode + 35) = (UINT64)LoadLibraryWAddress;
			UINT32	LoadLibraryAddressOffset = (UINT32)(((PUINT8)BufferData + 35) - ((PUINT8)BufferData + 11) - 6);
			*(PUINT32)(ShellCode + 13) = LoadLibraryAddressOffset;

			// ���� rip ��ַ
			*(PUINT64)(ShellCode + 27) = ThreadContext.Rip;

			if (!WriteProcessMemory(ProcessHandle, BufferData, ShellCode, sizeof(ShellCode), NULL))
			{
				return FALSE;
			}
			ThreadContext.Rip = (UINT64)BufferData;

#else
			PUINT8	v1 = ShellCode + 29;

			memcpy((char*)v1, DllFullPath, (wcslen(DllFullPath) + 1) * sizeof(WCHAR));	//������Ҫע���DLL����
			*(PUINT32)(ShellCode + 3) = (UINT32)BufferData + 29;

			*(PUINT32)(ShellCode + 25) = LoadLibraryWAddress;   //loadlibrary��ַ����shellcode��
			*(PUINT32)(ShellCode + 9) = (UINT32)BufferData + 25;//�޸�call ֮��ĵ�ַ ΪĿ��ռ��� loaddlladdr�ĵ�ַ
																//////////////////////////////////
			*(PUINT32)(ShellCode + 21) = ThreadContext.Eip;
			*(PUINT32)(ShellCode + 17) = (UINT32)BufferData + 21;//�޸�jmp ֮��Ϊԭ��eip�ĵ�ַ
			if (!WriteProcessMemory(ProcessHandle, BufferData, ShellCode, sizeof(ShellCode), NULL))
			{
				printf("write Process Error\n");
				return FALSE;
			}
			ThreadContext.Eip = (UINT32)BufferData;

#endif			
			if (!SetThreadContext(ThreadHandle, &ThreadContext))
			{
				printf("set thread context error\n");
				return FALSE;
			}
			ResumeThread(ThreadHandle);


			printf("ShellCode ע�����\r\n");
		}
	}

	CloseHandle(ThreadHandle);
	CloseHandle(ProcessHandle);
	return TRUE;
}

/************************************************************************
*  Name : GetLoadLibraryAddressInTargetProcessImportTable
*  Param: ProcessId					����Id							��IN��
*  Param: ImportFunctionAddress		LoadLibraryWĿ����̵�����е�ַ��OUT��
*  Ret  : BOOL
*  ReadProcessMemory��ȡĿ�����ģ�飬�����������õ��뺯��LoadLibraryW��ַ
************************************************************************/

BOOL GetLoadLibraryAddressInTargetProcessImportTable(IN UINT32 ProcessId, OUT PUINT_PTR ImportFunctionAddress)
{
	BOOL					bOk = FALSE;
	INT						i = 0, j = 0;
	HANDLE					ProcessHandle = NULL;
	PEB						Peb = { 0 };
	UINT_PTR				ModuleBase = 0;
	IMAGE_DOS_HEADER		DosHeader = { 0 };
	IMAGE_NT_HEADERS		NtHeader = { 0 };
	IMAGE_IMPORT_DESCRIPTOR	ImportDescriptor = { 0 };
	CHAR					szImportModuleName[MAX_PATH] = { 0 };			// ����ģ������
	IMAGE_THUNK_DATA		OriginalFirstThunk = { 0 };
	PIMAGE_IMPORT_BY_NAME	ImageImportByName = NULL;
	CHAR					szImportFunctionName[MAX_PATH] = { 0 };			// ���Ƶ��뺯������
	UINT32					ImportDescriptorRVA = 0;


	// ͨ������Id���Ŀ�����Peb
	bOk = GetPebByProcessId(ProcessId, &Peb);
	if (bOk == FALSE)
	{
		return FALSE;
	}

	ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
	if (ProcessHandle == NULL)
	{
		return FALSE;
	}

	ModuleBase = (UINT_PTR)Peb.ImageBaseAddress;

	ReadProcessMemory(ProcessHandle, (PVOID)ModuleBase, &DosHeader, sizeof(IMAGE_DOS_HEADER), NULL);
	ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + DosHeader.e_lfanew), &NtHeader, sizeof(IMAGE_NT_HEADERS), NULL);

	ImportDescriptorRVA = NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

	// ����ÿһ��

	for (i = 0, ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptorRVA), &ImportDescriptor, sizeof(IMAGE_IMPORT_DESCRIPTOR), NULL);
		ImportDescriptor.FirstThunk != 0;
		++i, ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptorRVA + i * sizeof(IMAGE_IMPORT_DESCRIPTOR)), &ImportDescriptor, sizeof(IMAGE_IMPORT_DESCRIPTOR), NULL))
	{
		// ��ȡ�����

		if (ImportDescriptor.OriginalFirstThunk == 0 && ImportDescriptor.FirstThunk == 0)
		{
			break;
		}

		// ��ȡ����ģ����
		ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptor.Name), szImportModuleName, MAX_PATH, NULL);

		if (_stricmp(szImportModuleName, "Kernel32.dll") == 0)
		{
			// Ŀ��ģ���ҵ��ˣ���ʼ����IAT INT
			for (j = 0, ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptor.OriginalFirstThunk), &OriginalFirstThunk, sizeof(IMAGE_THUNK_DATA), NULL);
				/*OriginalFirstThunk.u1.AddressOfData != 0*/;
				++j, ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptor.OriginalFirstThunk + j * sizeof(IMAGE_THUNK_DATA)), &OriginalFirstThunk, sizeof(IMAGE_THUNK_DATA), NULL))
			{
				// ��ŵ���Ĳ�����
				if (IMAGE_SNAP_BY_ORDINAL(OriginalFirstThunk.u1.Ordinal))
				{
					continue;
				}

				// ���Ƶ���ĺ�������
				ImageImportByName = (PIMAGE_IMPORT_BY_NAME)((PUINT8)ModuleBase + OriginalFirstThunk.u1.AddressOfData);
				ReadProcessMemory(ProcessHandle, ImageImportByName->Name, szImportFunctionName, MAX_PATH, NULL);

				// ���뺯����ַ
				ReadProcessMemory(ProcessHandle, (PVOID)((PUINT8)ModuleBase + ImportDescriptor.FirstThunk + j * sizeof(IMAGE_THUNK_DATA)), ImportFunctionAddress, sizeof(UINT_PTR), NULL);

				if (_stricmp(szImportFunctionName, "LoadLibraryW") == 0)		// ���Է��֣�ֻ�ҵ���W����
				{
					// Hit!
					//	MessageBoxA(0, 0, 0, 0);
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}


/************************************************************************
*  Name : GetPebByProcessId
*  Param: ProcessId			����Id		��IN��
*  Param: Peb				PEB�ṹ��	��OUT��
*  Ret  : BOOL
*  NtQueryInformationProcess+ProcessBasicInformation���Peb����ַ
************************************************************************/

BOOL GetPebByProcessId(IN UINT32 ProcessId, OUT PPEB Peb)
{
	BOOL						bOk = FALSE;
	NTSTATUS					Status = 0;
	HANDLE						ProcessHandle = NULL;
	UINT32						ReturnLength = 0;
	SIZE_T						NumberOfBytesRead = 0;
	PROCESS_BASIC_INFORMATION	pbi = { 0 };

	ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
	if (ProcessHandle == NULL)
	{
		return FALSE;
	}

	pfnNtQueryInformationProcess	NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationProcess");
	if (NtQueryInformationProcess == NULL)
	{
		CloseHandle(ProcessHandle);
		ProcessHandle = NULL;
		return FALSE;
	}

	// ͨ�� NtQueryInformationProcess ��� ProcessBasicInformation

	Status = NtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &ReturnLength);
	if (!NT_SUCCESS(Status))
	{
		CloseHandle(ProcessHandle);
		ProcessHandle = NULL;
		return FALSE;
	}

	// ͨ��ReadProcessMemory �ӽ������� PebBaseAddress �ڴ����ݶ�ȡ����

	bOk = ReadProcessMemory(ProcessHandle, pbi.PebBaseAddress, Peb, sizeof(PEB), &NumberOfBytesRead);
	if (bOk == FALSE)
	{
		CloseHandle(ProcessHandle);
		ProcessHandle = NULL;
		return FALSE;
	}

	CloseHandle(ProcessHandle);
	return TRUE;
}

/************************************************************************
*  Name : GetThreadIdByProcessId
*  Param: ProcessId			����Id		��IN��
*  Param: ThreadId			�߳�Id		��OUT��
*  Ret  : BOOL
*  ZwQuerySystemInformation+SystemProcessInformation��ý��������Ϣ�Ӷ��õ�һ���߳�Id
************************************************************************/

BOOL GetThreadIdByProcessId(IN UINT32 ProcessId, OUT PUINT32 ThreadId)
{
	BOOL						bOk = FALSE;
	NTSTATUS					Status = 0;
	PVOID						BufferData = NULL;
	PSYSTEM_PROCESS_INFO		spi = NULL;
	pfnZwQuerySystemInformation ZwQuerySystemInformation = NULL;

	ZwQuerySystemInformation = (pfnZwQuerySystemInformation)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwQuerySystemInformation");
	if (ZwQuerySystemInformation == NULL)
	{
		return FALSE;
	}

	BufferData = malloc(1024 * 1024);
	if (!BufferData)
	{
		return FALSE;
	}

	// ��QuerySystemInformationϵ�к����У���ѯSystemProcessInformationʱ��������ǰ������ڴ棬�����Ȳ�ѯ�õ����������µ���
	Status = ZwQuerySystemInformation(SystemProcessInformation, BufferData, 1024 * 1024, NULL);
	if (!NT_SUCCESS(Status))
	{
		free(BufferData);
		return FALSE;
	}

	spi = (PSYSTEM_PROCESS_INFO)BufferData;

	// �������̣��ҵ����ǵ�Ŀ�����
	while (TRUE)
	{
		bOk = FALSE;
		if (spi->UniqueProcessId == (HANDLE)ProcessId)
		{
			bOk = TRUE;
			break;
		}
		else if (spi->NextEntryOffset)
		{
			spi = (PSYSTEM_PROCESS_INFO)((PUINT8)spi + spi->NextEntryOffset);
		}
		else
		{
			break;
		}
	}

	if (bOk)
	{
		for (INT i = 0; i < spi->NumberOfThreads; i++)
		{
			// �����ҵ����߳�Id
			*ThreadId = (UINT32)spi->Threads[i].ClientId.UniqueThread;
			break;
		}
	}

	if (BufferData != NULL)
	{
		free(BufferData);
	}

	return bOk;
}


/************************************************************************
*  Name : GrantPriviledge
*  Param: Priviledge			������Ȩ��
*  Ret  : BOOL
*  ����ntdll��������RtlAdjustPrivilege��Ȩ
************************************************************************/

BOOL GrantPriviledge(IN UINT32 Priviledge)
{
	pfnRtlAdjustPrivilege	RtlAdjustPrivilege = NULL;
	BOOLEAN					WasEnable = FALSE;

	RtlAdjustPrivilege = (pfnRtlAdjustPrivilege)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlAdjustPrivilege");
	if (RtlAdjustPrivilege == NULL)
	{
		return FALSE;
	}

	RtlAdjustPrivilege(Priviledge, TRUE, FALSE, &WasEnable);

	return TRUE;
}