//WinTime - A time utility for Windows
#define _WIN32_WINNT 0x0600
#include <Windows.h>
#include <timeapi.h>
#include <processthreadsapi.h>
#include <realtimeapiset.h>

#include "./fmt.h"
#include <cstdint>
using uint64 = uint64_t;

template <typename T> T to(FILETIME& i);
template<> uint64 to<uint64>(FILETIME& i){
	return std::bit_cast<ULARGE_INTEGER, FILETIME>(i).QuadPart;
};

class Program{
	size_t runs;
	size_t warmup;
	double real = 0;
	double kernel = 0;
	double user = 0;
	double cycle = 0;
	bool echo = false;

	uint64 exitCode = 0;

	int error = 0;

	public:
	Program(size_t warmup, size_t runs): warmup(warmup), runs(runs){};

	inline int time(LPTSTR inputStart){
		timeBeginPeriod(1);

		STARTUPINFO startupInfo{};
		startupInfo.cb = sizeof(STARTUPINFO);

		if(!echo){
			startupInfo.dwFlags |= STARTF_USESTDHANDLES;
		}
		
		PROCESS_INFORMATION processInfo{};

		if(!CreateProcess(nullptr, inputStart, nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo)){
			println("Error: Failed to create the process.");
			error = 2;
			return 0;
		}

		//Wait for the process to exit.
		if(WaitForSingleObject(processInfo.hProcess, INFINITE) != WAIT_OBJECT_0){
			println("Error: Failed to wait for the process.");
			error = 3;
			return 0;
		}

		collect(processInfo.hProcess);

		//Cleanup
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		timeEndPeriod(1);
		return 0;
	};

	inline void collect(HANDLE process){
		//Process exit code
		DWORD dwExitCode;
		if(!GetExitCodeProcess(process, &dwExitCode)){ return exit(); }
		exitCode = dwExitCode;

		//CPU times
		FILETIME creationTime;
		FILETIME exitTime;
		FILETIME kernelTime;
		FILETIME userTime;
		if(!GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)){ return exit(); }

		real += to<uint64>(exitTime) - to<uint64>(creationTime);
		kernel += to<uint64>(kernelTime);
		user += to<uint64>(userTime);
		
		//Cycle time
		ULONG64 cycleTime;
		if(!QueryProcessCycleTime(process, &cycleTime)){ return exit(); }

		cycle += cycleTime;
	};

	inline void printInfo(){
		double timeFactor = 1.0e-7 / runs;
		println("Real time:   {:.6f}s", timeFactor * real);
		//println("Cycle time:  {:.6f}s", cycleTime / runs);
		println("Kernel time: {:.6f}s ({:.1f}%)", timeFactor * kernel, 100.0 * kernel / real);
		println("User time:   {:.6f}s ({:.1f}%)", timeFactor * user, 100.0 * user / real);
	};

	inline void exit(){ error = 1; };
};

inline bool isWhitespace(auto* c){
	return *c == ' ' || *c == '\t';
};

//Increment the pointer until a non-whitespace character is hit
inline void skipWhitespace(auto*& c){
	while(isWhitespace(c)){ ++c; }
};

inline void findWhitespace(auto*& c){
	while(*c && !isWhitespace(c)){ ++c; }
};

int main(int argc, char** argv){
	auto cli = GetCommandLine();
	auto inputStart = cli;

	if(*inputStart == '"'){
		while(*++inputStart != '"');
	}
	findWhitespace(inputStart);
	skipWhitespace(inputStart);

	while(*inputStart == '-'){
		//Switch on the supported command line arguments
	};

	skipWhitespace(inputStart);

	println("Hello, World!");
	println(L"{}", cli);

	Program program{0, 1};
	auto res = program.time(inputStart);
	program.printInfo();
	return res;
};