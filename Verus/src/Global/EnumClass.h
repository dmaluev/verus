// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr U operator&(T a, T b)
	{
		return static_cast<U>(a) & static_cast<U>(b);
	}

	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr T operator|(T a, T b)
	{
		return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
	}

	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr T& operator&=(T& a, T b)
	{
		a = static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
		return a;
	}

	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr T& operator|=(T& a, T b)
	{
		a = static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
		return a;
	}

	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr T operator~(T a)
	{
		return static_cast<T>(~static_cast<U>(a));
	}

	template<typename T, typename = std::enable_if_t<std::is_enum<T>::value>, typename U = std::underlying_type_t<T>>
	constexpr U operator+(T a)
	{
		return static_cast<U>(a);
	}
}
