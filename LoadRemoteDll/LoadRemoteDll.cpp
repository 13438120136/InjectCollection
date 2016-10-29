// LoadRemoteDll.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "LoadRemoteDll.h"

#pragma intrinsic(_ReturnAddress)

__declspec(noinline)
UINT_PTR caller()
{
	return (UINT_PTR)_ReturnAddress();		// #include <intrin.h>
}

typedef
HMODULE
(WINAPI * pfnLoadLibraryA)(LPCSTR lpLibFileName);

typedef
FARPROC
(WINAPI * pfnGetProcAddress)(HMODULE hModule, LPCSTR lpProcName);

typedef
LPVOID
(WINAPI * pfnVirtualAlloc)(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);

typedef
LONG	// NTSTATUS
(NTAPI * pfnNtFlushInstructionCache)(HANDLE ProcessHandle, PVOID BaseAddress, SIZE_T Length);

typedef
BOOL
(APIENTRY * pfnDllMain)(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved);


LOADREMOTEDLL_API UINT_PTR WINAPI LoadDllByOEP(PVOID lParam)
{

	UINT_PTR			LibraryAddress = 0;

	PIMAGE_DOS_HEADER	DosHeader = NULL;
	PIMAGE_NT_HEADERS	NtHeader = NULL;

	pfnLoadLibraryA				LoadLibraryAAddress = NULL;
	pfnGetProcAddress			GetProcAddressAddress = NULL;
	pfnVirtualAlloc				VirtualAllocAddress = NULL;
	pfnNtFlushInstructionCache	NtFlushInstructionCacheAddress = NULL;


	LibraryAddress = caller();		// �����һ��ָ��ĵ�ַ����ʵ����Ϊ�˻�õ�ǰָ���ַ��Ϊ����Ѱ��PEͷ�ṩ���
	DosHeader = (PIMAGE_DOS_HEADER)LibraryAddress;

	while (TRUE)
	{
		if (DosHeader->e_magic == IMAGE_DOS_SIGNATURE &&
			DosHeader->e_lfanew >= sizeof(IMAGE_DOS_HEADER) &&
			DosHeader->e_lfanew < 1024)
		{
			NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)LibraryAddress + DosHeader->e_lfanew);
			if (NtHeader->Signature == IMAGE_NT_SIGNATURE)
			{
				break;
			}
		}
		LibraryAddress--;
		DosHeader = (PIMAGE_DOS_HEADER)LibraryAddress;
	}

	// ���PEB
#ifdef _WIN64
	PPEB Peb = (PPEB)__readgsqword(0x60);
#else
	PPEB Peb = (PPEB)__readfsdword(0x30);
#endif

	PPEB_LDR_DATA Ldr = Peb->Ldr;

	// 1.��Dll�������л�ȡ������ַ

	for (PLIST_ENTRY TravelListEntry = (PLIST_ENTRY)Ldr->InLoadOrderModuleList.Flink;
		TravelListEntry != &Ldr->InLoadOrderModuleList;		// ��ͷ�ڵ�
		TravelListEntry = TravelListEntry->Flink)

	{
		PLDR_DATA_TABLE_ENTRY	LdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)TravelListEntry;

		UINT32	FunctionCount = 0;

		//		WCHAR*	DllName = (WCHAR*)LdrDataTableEntry->BaseDllName.Buffer;

		UINT_PTR	DllName = (UINT_PTR)LdrDataTableEntry->BaseDllName.Buffer;

		UINT32	DllLength = LdrDataTableEntry->BaseDllName.Length;

		UINT_PTR	DllBaseAddress = (UINT_PTR)LdrDataTableEntry->DllBase;

		DosHeader = (PIMAGE_DOS_HEADER)DllBaseAddress;
		NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)DllBaseAddress + DosHeader->e_lfanew);

		IMAGE_DATA_DIRECTORY	ExportDataDirectory = (IMAGE_DATA_DIRECTORY)(NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
		PIMAGE_EXPORT_DIRECTORY	ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((PUINT8)DllBaseAddress + ExportDataDirectory.VirtualAddress);
		PUINT32					AddressOfFunctions = (PUINT32)((PUINT8)DllBaseAddress + ExportDirectory->AddressOfFunctions);
		PUINT32					AddressOfNames = (PUINT32)((PUINT8)DllBaseAddress + ExportDirectory->AddressOfNames);
		PUINT16					AddressOfNameOrdinals = (PUINT16)((PUINT8)DllBaseAddress + ExportDirectory->AddressOfNameOrdinals);

		UINT16					Ordinal = 0;
		UINT_PTR				ExportFunctionAddress = 0;

		UINT32					HashValue = 0;

		// ��Dll����ת����Hashֵ
		do
		{
			HashValue = ror((UINT32)HashValue);

			if (*((PUINT8)DllName) >= 'a')
			{
				HashValue += *((PUINT8)DllName) - 0x20;
			}
			else
			{
				HashValue += *((PUINT8)DllName);
			}
			DllName++;
		} while (--DllLength);



		if (HashValue == KERNEL32DLL_HASH)
		{
			FunctionCount = 3;

			for (INT i = 0; i < ExportDirectory->NumberOfFunctions; i++)
			{
				if (FunctionCount == 0)
				{
					break;
				}

				CHAR* szExportFunctionName = (CHAR*)((PUINT8)DllBaseAddress + AddressOfNames[i]);

				HashValue = hash(szExportFunctionName);

				if (HashValue == LOADLIBRARYA_HASH)
				{
					Ordinal = AddressOfNameOrdinals[i];
					LoadLibraryAAddress = (pfnLoadLibraryA)((PUINT8)DllBaseAddress + AddressOfFunctions[Ordinal]);
					FunctionCount--;
				}
				else if (HashValue == GETPROCADDRESS_HASH)
				{
					Ordinal = AddressOfNameOrdinals[i];
					GetProcAddressAddress = (pfnGetProcAddress)((PUINT8)DllBaseAddress + AddressOfFunctions[Ordinal]);
					FunctionCount--;
				}
				else if (HashValue == VIRTUALALLOC_HASH)
				{
					Ordinal = AddressOfNameOrdinals[i];
					VirtualAllocAddress = (pfnVirtualAlloc)((PUINT8)DllBaseAddress + AddressOfFunctions[Ordinal]);
					FunctionCount--;
				}
			}
		}
		else if (HashValue == NTDLLDLL_HASH)
		{
			FunctionCount = 1;

			for (INT i = 0; i < ExportDirectory->NumberOfFunctions; i++)
			{
				if (FunctionCount == 0)
				{
					break;
				}

				CHAR* szExportFunctionName = (CHAR*)((PUINT8)DllBaseAddress + AddressOfNames[i]);

				HashValue = hash(szExportFunctionName);

				if (HashValue == NTFLUSHINSTRUCTIONCACHE_HASH)
				{
					Ordinal = AddressOfNameOrdinals[i];
					NtFlushInstructionCacheAddress = (pfnNtFlushInstructionCache)((PUINT8)DllBaseAddress + AddressOfFunctions[Ordinal]);
					FunctionCount--;
				}
			}
		}


		if (LoadLibraryAAddress != NULL &&
			GetProcAddressAddress != NULL &&
			VirtualAllocAddress != NULL &&
			NtFlushInstructionCacheAddress != NULL)
		{
			break;
		}
	}

	// 2.�����ڴ棬���¼������ǵ�Dll

	// �ٴθ���DosHeader��NtHeader
	DosHeader = (PIMAGE_DOS_HEADER)LibraryAddress;
	NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)LibraryAddress + DosHeader->e_lfanew);

	// ���������ڴ棨SizeOfImage����PE���ڴ��еĴ�С��

	/*	_asm
	{
	int 3;
	}
	*/
	// ����Լ����������ͷָ�벻������ƶ���ʹ��һ�����������
	UINT_PTR NewBaseAddress = (UINT_PTR)VirtualAllocAddress(NULL, NtHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	UINT_PTR OldPtr = LibraryAddress;
	UINT_PTR BasePtr = NewBaseAddress;


	// 2.1���ȿ���ͷ + �ڱ�
	UINT32	SizeOfHeaders = NtHeader->OptionalHeader.SizeOfHeaders;
	while (SizeOfHeaders--)
	{
		*(PUINT8)BasePtr++ = *(PUINT8)OldPtr++;
	}
	//	memcpy((PVOID)NewBaseAddress, (PVOID)LibraryAddress, NtHeader->OptionalHeader.SizeOfHeaders);

	/*
	PIMAGE_SECTION_HEADER	SectionHeader = (PIMAGE_SECTION_HEADER)((PUINT8)&NtHeader->OptionalHeader + NtHeader->FileHeader.SizeOfOptionalHeader);
	UINT32					NumberOfSections = NtHeader->FileHeader.NumberOfSections;
	while (NumberOfSections--)
	{
		UINT_PTR	NewSectionAddress = (UINT_PTR)((PUINT8)NewBaseAddress + SectionHeader->VirtualAddress);
		UINT_PTR	OldSectionAddress = (UINT_PTR)((PUINT8)LibraryAddress + SectionHeader->PointerToRawData);
		UINT32 SizeOfRawData = SectionHeader->SizeOfRawData;
		while (SizeOfRawData--)
		{
			*(PUINT8)NewSectionAddress++ = *(PUINT8)OldSectionAddress++;
		}
		SectionHeader = (PIMAGE_SECTION_HEADER)((PUINT8)SectionHeader + sizeof(IMAGE_SECTION_HEADER));
	}
	*/

	// 2.2��������
	PIMAGE_SECTION_HEADER	SectionHeader = IMAGE_FIRST_SECTION(NtHeader);
	for (INT i = 0; i < NtHeader->FileHeader.NumberOfSections; i++)
	{
		if (SectionHeader[i].VirtualAddress == 0 || SectionHeader[i].SizeOfRawData == 0)	// �ڿ�����û������
		{
			continue;
		}

		// ��λ�ýڿ����ڴ��е�λ��
		UINT_PTR	NewSectionAddress = (UINT_PTR)((PUINT8)NewBaseAddress + SectionHeader[i].VirtualAddress);
		UINT_PTR	OldSectionAddress = (UINT_PTR)((PUINT8)LibraryAddress + SectionHeader[i].PointerToRawData);
		// ���ƽڿ����ݵ������ڴ�
		UINT32 SizeOfRawData = SectionHeader[i].SizeOfRawData;
		while (SizeOfRawData--)
		{
			*(PUINT8)NewSectionAddress++ = *(PUINT8)OldSectionAddress++;
		}
		//memcpy(SectionAddress, (PVOID)((PUINT8)LibraryAddress + SectionHeader[i].PointerToRawData), SectionHeader[i].SizeOfRawData);
	}
	
	// 2.3���������(IAT)
	IMAGE_DATA_DIRECTORY		ImportDataDirectory = (IMAGE_DATA_DIRECTORY)(NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]);
	PIMAGE_IMPORT_DESCRIPTOR	ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((PUINT8)NewBaseAddress + ImportDataDirectory.VirtualAddress);

	/*	
	_asm
	{
		int 3;
	}
	*/
/*
	while (ImportDescriptor->Characteristics != 0)
	{
		PIMAGE_THUNK_DATA	FirstThunk = (PIMAGE_THUNK_DATA)((PUINT8)NewBaseAddress + ImportDescriptor->FirstThunk);
		PIMAGE_THUNK_DATA	OriginalFirstThunk = (PIMAGE_THUNK_DATA)((PUINT8)NewBaseAddress + ImportDescriptor->OriginalFirstThunk);

		// ��ȡ����ģ������
		//	char	szModuleName[MAX_PATH] = { 0 };

		PCHAR	ModuleName = (PCHAR)((PUINT8)NewBaseAddress + ImportDescriptor->Name);

		HMODULE	Dll = LoadLibraryAAddress(ModuleName);

		UINT_PTR			FunctionAddress = 0;

		for (INT i = 0; OriginalFirstThunk[i].u1.Function != 0; i++)
		{
			if (IMAGE_SNAP_BY_ORDINAL(OriginalFirstThunk[i].u1.Ordinal))
			{
				FunctionAddress = (UINT_PTR)GetProcAddressAddress(Dll, MAKEINTRESOURCEA((IMAGE_ORDINAL(OriginalFirstThunk[i].u1.Ordinal))));
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME	ImageImportByName = (PIMAGE_IMPORT_BY_NAME)((PUINT8)NewBaseAddress + OriginalFirstThunk[i].u1.AddressOfData);
				FunctionAddress = (UINT_PTR)GetProcAddressAddress(Dll, (CHAR*)ImageImportByName->Name);		// ͨ���������Ƶõ�������ַ
			}
			FirstThunk[i].u1.Function = FunctionAddress;
		}
		ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((PUINT8)ImportDescriptor + sizeof(IMAGE_IMPORT_DESCRIPTOR));
	}

*/
	
	for (INT i = 0; ImportDescriptor[i].Name != NULL; i++)
	{
		// ���ص��붯̬��
		HMODULE	Dll = LoadLibraryAAddress((const CHAR*)((PUINT8)NewBaseAddress + ImportDescriptor[i].Name));

		PIMAGE_THUNK_DATA	OriginalFirstThunk = (PIMAGE_THUNK_DATA)((PUINT8)NewBaseAddress + ImportDescriptor[i].OriginalFirstThunk);
		PIMAGE_THUNK_DATA	FirstThunk = (PIMAGE_THUNK_DATA)((PUINT8)NewBaseAddress + ImportDescriptor[i].FirstThunk);
		UINT_PTR			FunctionAddress = 0;

		// ����ÿ������ģ��ĺ���
		for (INT j = 0; OriginalFirstThunk[j].u1.Function; j++)
		{
			if (&OriginalFirstThunk[j] && IMAGE_SNAP_BY_ORDINAL(OriginalFirstThunk[j].u1.Ordinal))
			{
				// ��ŵ���---->����ֱ�Ӵ�Dll�ĵ��������ҵ�������ַ
				//	FunctionAddress = (UINT_PTR)GetProcAddressAddress(Dll, MAKEINTRESOURCEA((IMAGE_ORDINAL(OriginalFirstThunk[j].u1.Ordinal))));		// ��ȥ���λ��Ϊ���

				DosHeader = (PIMAGE_DOS_HEADER)Dll;
				NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)Dll + DosHeader->e_lfanew);

				PIMAGE_EXPORT_DIRECTORY ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((PUINT8)Dll + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

				// ����������ַRVA����
				PUINT32	AddressOfFunctions = (PUINT32)((PUINT8)Dll + ExportDirectory->AddressOfFunctions);

				UINT16	Ordinal = IMAGE_ORDINAL(OriginalFirstThunk[j].u1.Ordinal - ExportDirectory->Base);		// ����������� - Base(����������ŵ���ʼֵ) = ���������ں�����ַ�������

				FunctionAddress = (UINT_PTR)((PUINT8)Dll + AddressOfFunctions[Ordinal]);
			}
			else
			{
				// ���Ƶ���
				PIMAGE_IMPORT_BY_NAME	ImageImportByName = (PIMAGE_IMPORT_BY_NAME)((PUINT8)NewBaseAddress + OriginalFirstThunk[j].u1.AddressOfData);
				FunctionAddress = (UINT_PTR)GetProcAddressAddress(Dll, (CHAR*)ImageImportByName->Name);		// ͨ���������Ƶõ�������ַ
			}
			// ����IAT
			FirstThunk[j].u1.Function = FunctionAddress;
		}
	}



	// 2.4�����ض����
	DosHeader = (PIMAGE_DOS_HEADER)LibraryAddress;
	NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)LibraryAddress + DosHeader->e_lfanew);

	//	UINT_PTR Delta = NewBaseAddress - NtHeader->OptionalHeader.ImageBase;

	IMAGE_DATA_DIRECTORY	BaseRelocDataDirectory = (IMAGE_DATA_DIRECTORY)(NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);

	// �����ض����
	if (BaseRelocDataDirectory.Size != 0)
	{
		PIMAGE_BASE_RELOCATION	BaseRelocation = (PIMAGE_BASE_RELOCATION)((PUINT8)NewBaseAddress + BaseRelocDataDirectory.VirtualAddress);

		while (BaseRelocation->SizeOfBlock != 0)
		{
			typedef struct _IMAGE_RELOC
			{
				UINT16	Offset : 12;		// ��12λ---ƫ��
				UINT16	Type : 4;			// ��4λ---����
			} IMAGE_RELOC, *PIMAGE_RELOC;

			// ��λ���ض�λ��
			PIMAGE_RELOC RelocationBlock = (PIMAGE_RELOC)((PUINT8)BaseRelocation + sizeof(IMAGE_BASE_RELOCATION));
			// ������Ҫ�������ض���λ�����Ŀ
			UINT32	NumberOfRelocations = (BaseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(UINT16);

			for (INT i = 0; i < NumberOfRelocations; i++)
			{
				if (RelocationBlock[i].Type == IMAGE_REL_BASED_DIR64)
				{
					// 64 λ
					PUINT64	Address = (PUINT64)((PUINT8)NewBaseAddress + BaseRelocation->VirtualAddress + RelocationBlock[i].Offset);
					UINT64	Delta = (UINT64)NewBaseAddress - NtHeader->OptionalHeader.ImageBase;
					*Address += Delta;

				}
				else if (RelocationBlock[i].Type == IMAGE_REL_BASED_HIGHLOW)
				{
					// 32 λ
					PUINT32	Address = (PUINT32)((PUINT8)NewBaseAddress + BaseRelocation->VirtualAddress + (RelocationBlock[i].Offset));
					UINT32	Delta = (UINT32)NewBaseAddress - NtHeader->OptionalHeader.ImageBase;
					*Address += Delta;
				}
			}
			// ת����һ���ض����
			BaseRelocation = (PIMAGE_BASE_RELOCATION)((PUINT8)BaseRelocation + BaseRelocation->SizeOfBlock);
		}
	}

	// 3.���ģ��OEP

	UINT_PTR AddressOfEntryPoint = (UINT_PTR)((PUINT8)NewBaseAddress + NtHeader->OptionalHeader.AddressOfEntryPoint);

	NtFlushInstructionCacheAddress(INVALID_HANDLE_VALUE, NULL, 0);

	// ����ͨ��OEPȥ����DllMain
	((pfnDllMain)AddressOfEntryPoint)((HMODULE)NewBaseAddress, DLL_PROCESS_ATTACH, lParam);

	/*	_asm
	{
	int 3;
	}
	*/
	return AddressOfEntryPoint;
}