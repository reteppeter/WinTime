#pragma once
#include <fmt/core.h>
#include <fmt/format.h>
using fmt::print;
using fmt::format;

template <typename... Args>
inline void println(Args&&... args){
	print(args...);
	print("\n");
};