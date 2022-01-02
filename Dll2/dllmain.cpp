// specialization exploit from extra.dll
#include "pch.h"
#include <detours.h>
#include <cstdint>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include "ByteBuffer.h"
#pragma comment(lib, "Ws2_32.lib")
HMODULE hMod = NULL;

#define wow ((DWORD_PTR)(GetModuleHandle(0))) // base address of the wow module in memory


INT(__stdcall* Send)(SOCKET s, const char* buf, int len, int flags) = send; // original WS2 send
int SendH(SOCKET s, uint8_t* buf, int len, int flags) // the detour
{
	if (len < 6)
		return Send(s, (char*)buf, len, flags);

	// cast spell packet
	if ((int)buf[5] + (int)buf[4] == 0x8E + 0x32)
	{

		ByteBuffer bf(buf, len);

		__int64 unk00 = bf.Extract<__int64>();
		__int64 unk01 = bf.Extract<__int64>();
		int unk02 = bf.Extract<int>();
		int visualId = bf.Extract<int>();
		int spellId = bf.Extract<int>();
		int misc2 = bf.Extract<int>();
		int misc1 = bf.Extract<int>();
		// rest is (in reverse order): object guid + masks, opcode, packet guid


		if (spellId == 200749)
		{
			std::cout << "detected change spec cast" << std::endl;
			// correct guid
			misc1 = 0x4A; // FEROCITY

			// reinsert
			bf << misc1;
			bf << misc2;
			bf << spellId;
			bf << visualId;
			bf << unk02;
			bf << unk01;
			bf << unk00;
			return Send(s, (char*)bf.GetBytes(), bf.GetSize(), flags);
		}
		return Send(s, (char*)buf, len, flags);
	}

	return Send(s, (char*)buf, len, flags);
}

bool attached = false;
void Attach()
{
	if (attached)
		return;

	std::cout << "detoured ws2_32.send at " << &Send << " to " << &SendH << std::endl;
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)Send, SendH);
	DetourTransactionCommit();
	attached = true;
}

void Detach()
{
	if (!attached)
		return;

	std::cout << "detaching detours" << std::endl;
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)Send, SendH);
	DetourTransactionCommit();
	attached = false;
}

bool conBreak = false;
void HandleCommand(std::string comm)
{
	if (comm == "enable")
	{
		Attach();
		std::cout << "spec mod is now enabled, change your specialization and it will bug your character" << std::endl;
	}

	if (comm == "quit")
	{
		Detach();
		auto w = GetConsoleWindow();
		std::cout << "this window is now safe to close (if it doesn't close automatically)" << std::endl;
		FreeConsole();
		PostMessage(w, WM_CLOSE, 0, 0);
		Sleep(20);
		PostMessage(w, WM_CLOSE, 0, 0);
		FreeLibraryAndExitThread(hMod, 1);
	}

	else
	{
		std::cout << "invalid command" << std::endl;
	}
}

DWORD WINAPI hMain(LPVOID h)
{
	AllocConsole();
	auto f = freopen("CONOUT$", "w", stdout);
	f = freopen("CONIN$", "r", stdin);
	hMod = (HMODULE)h;
	Attach();
	std::cout << "console is now accepting input" << std::endl;
	std::cout << "type 'enable' to enable bug, type 'quit' to close this window & eject dll" << std::endl;
	std::string in = "";
	while (!conBreak)
	{
		std::getline(std::cin, in);
		if (in != "")
		{
			HandleCommand(in);
		}
		in = "";
	}
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		CloseHandle(CreateThread(nullptr, 0, hMain, hModule, 0, nullptr));
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

