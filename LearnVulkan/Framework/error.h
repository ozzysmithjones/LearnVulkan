#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <stacktrace>

template<typename ... Ts>
void log_error(Ts... error_messages)
{
#ifndef NDEBUG
	std::stringstream ss;
	((ss << error_messages), ...);

	std::string error = ss.str();
	std::cerr << error << "\nstack trace:\n" << std::stacktrace::current() << "\n\n";

#ifdef _MSC_VER
	__debugbreak();
#endif
#endif
}