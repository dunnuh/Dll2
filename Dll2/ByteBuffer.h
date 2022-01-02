#pragma once
#include <iostream>
#include <chrono>
#include <string>
#include <vector>

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
