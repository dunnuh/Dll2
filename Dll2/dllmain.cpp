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
#pragma comment(lib, "Ws2_32.lib")
HMODULE hMod = NULL;
class ByteBuffer
{
private:
	std::vector<uint8_t> bytes;

public:
	size_t GetSize()
	{
		return bytes.size();
	}

	uint8_t* GetBytes()
	{
		return &bytes[0];
	}

	void Empty()
	{
		bytes.clear();
	}

	template <typename T>
	void operator<<(T val)
	{
		AddValue(val);
	}

	template <typename T>
	void operator>>(T& val)
	{
		T b = Extract();
		val = b;
	}

	void operator<<(uint8_t byte)
	{
		bytes.push_back(byte);
	}



	void operator<<(std::vector<uint8_t> bytev)
	{
		AddArray(&bytes[0], bytes.size());
	}

	void AddArray(uint8_t* arr, size_t len)
	{
		for (size_t i = 0; i < len; i++)
		{
			bytes.push_back(arr[i]);
		}
	}

	void Pad(size_t length, uint8_t with = 0)
	{
		for (int i = 0; i < length; i++)
		{
			bytes.push_back(with);
		}
	}

	template <typename T>
	void AddValue(T value, int len = -1)
	{
		auto t_bytes = reinterpret_cast<unsigned char*>(&value);
		int size = len == -1 ? sizeof(value) : len;
		AddArray(t_bytes, size);
	}

	template <typename T>
	void AddValues(int count, ...)
	{
		va_list args;
		va_start(args, count);

		for (int i = 0; i < count; i++)
		{
			AddValue<T>(va_arg(args, T));
		}
		va_end(args);
	}


	void SetLast(uint8_t byte)
	{
		SetFromLast(0, byte);
	}

	void SetFromLast(int off, uint8_t byte)
	{
		int target = (GetSize() - 1) - off;
		if (target < 0)
			target = 0;
		GetBytes()[target] = byte;
	}

	template <class T>
	T Extract()
	{
		std::vector<uint8_t> v;
		size_t amount = sizeof(T);
		for (int i = 0; i < amount; i++)
		{
			if (bytes.size() == 0)
				break;

			size_t mi = bytes.size() - 1;
			uint8_t byte = bytes[mi];
			v.push_back(byte);
			bytes.pop_back();
		}

		std::reverse(v.begin(), v.end()); // ensure correct byte order so they can be re-inserted and casted properly
		return *reinterpret_cast<T*>(&v[0]);
	}

	std::vector<uint8_t> ExtractVector(size_t amount)
	{
		std::vector<uint8_t> v;
		for (size_t i = 0; i < amount; i++)
		{
			if (bytes.size() == 0)
				break;

			size_t mi = bytes.size() - 1;
			uint8_t byte = bytes[mi];
			v.push_back(byte);
			bytes.pop_back();
		}

		std::reverse(v.begin(), v.end()); // ensure correct byte order so they can be re-inserted and casted properly
		return v;
	}

	std::vector<uint8_t> ExtractUntil(size_t ind)
	{
		size_t bz = bytes.size();
		size_t toExtract = bz - ind;
		return ExtractVector(toExtract);
	}

	ByteBuffer() {}
	ByteBuffer(unsigned char* from, size_t size)
	{
		for (int i = 0; i < size; i++)
		{
			bytes.push_back(from[i]);
		}
	}


};

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

