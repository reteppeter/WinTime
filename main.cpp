//WinTime - A time utility for Windows
#define _WIN32_WINNT 0x0600
#include <Windows.h>
#include <timeapi.h>
#include <processthreadsapi.h>
#include <realtimeapiset.h>
#define PSAPI_VERSION 2
#include <Psapi.h>

#define brase break; case

#include "./fmt.h"
#include <cstdint>
using uint64 = uint64_t;
using int64 = int64_t;
#include <charconv>

#include <string>
using tstring = std::basic_string<TCHAR>;

template <typename T> T to(FILETIME& i);
template<> uint64 to<uint64>(FILETIME& i){
	return std::bit_cast<ULARGE_INTEGER, FILETIME>(i).QuadPart;
};

class Program{
	struct Process{
		PROCESS_INFORMATION processInfo{};

		int create(tstring inputStart, bool echo){
			STARTUPINFO startupInfo{};
			startupInfo.cb = sizeof(STARTUPINFO);
			if(!echo){
				startupInfo.dwFlags |= STARTF_USESTDHANDLES;
			}

			if(!CreateProcess(nullptr, inputStart.data(), nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo)){
				println("Error: Failed to create the process.");
				return 2;
			}

			return 0;
		};

		int wait(){
			if(WaitForSingleObject(processInfo.hProcess, INFINITE) != WAIT_OBJECT_0){
				println("Error: Failed to wait for the process.");
				return 3;
			}

			return 0;
		};

		bool createAndWait(tstring inputStart, bool echo){
			return create(inputStart, echo) || wait();
		};

		void close(){
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);
		};
	};

	double real = 0;
	double kernel = 0;
	double user = 0;
	double cycle = 0;

	double pageFaults = 0;
	double peakPagefileUsage = 0;
	double peakWorkingSetSize = 0;
	double peakPagedPoolUsage = 0;
	double peakNonPagedPoolUsage = 0;

	uint64 exitCode = 0;
	int error = 0;

	public:
	size_t runs = 0;
	size_t warmup = 0;
	bool echo = true;
	bool portable = false;

	Program() = default;

	inline int time(LPTSTR inputStart){
		timeBeginPeriod(1);

		for(int i = 0; i < warmup; ++i){
			Process p{};
			if(p.createAndWait(inputStart, false)){ return 1; }
			p.close();
		}

		auto lecho = echo;
		for(int i = 0; i < runs; ++i){
			Process p{};
			if(p.createAndWait(inputStart, lecho)){ return 1; }
			collect(p.processInfo.hProcess);
			p.close();
			lecho = false;
		}

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

		//Memory usage
		PROCESS_MEMORY_COUNTERS_EX counter{};
		if(!GetProcessMemoryInfo(process, reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counter), sizeof(counter))){ return exit(); }

		pageFaults += counter.PageFaultCount;
		peakPagefileUsage += counter.PeakPagefileUsage;
		peakPagedPoolUsage += counter.QuotaPeakPagedPoolUsage;
		peakNonPagedPoolUsage += counter.QuotaPeakNonPagedPoolUsage;
		peakWorkingSetSize += counter.PeakWorkingSetSize;
	};

	//Min - Avg - Max ?
	inline void printInfo(){
		double timeFactor = 1.0e-7 / runs;
		auto realTime = timeFactor * real;
		auto kernelTime = timeFactor * kernel;
		auto userTime = timeFactor * user;
		if(portable){
			print(
				"real {:.6f}\n"
				"user {:.6f}\n"
				"sys {:.6f}\n",
				realTime,
				userTime,
				kernelTime
			);
			return;
		}

		println("Real time:   {:.6f}s", realTime);
		println("User time:   {:.6f}s ({:.1f}%)", userTime, 100.0 * user / real);
		println("Kernel time: {:.6f}s ({:.1f}%)", kernelTime, 100.0 * kernel / real);
		//println("Cycle time:  {:.6f}s", timeFactor * cycle);

		println("Page faults: {:.2f}", pageFaults / runs);
		println("Peak pagefile usage: {:.2f} bytes", peakPagefileUsage / runs);
		println("Peak paged pool usage: {:.2f} bytes", peakPagedPoolUsage / runs);
		println("Peak non-paged pool usage: {:.2f} bytes", peakNonPagedPoolUsage / runs);
		println("Peak working set size: {:.2f} bytes", peakWorkingSetSize / runs);
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

	Program program;

	const std::unordered_map<char, int> arguments{
		{'e', 0},
		{'p', 0},
		{'w', 1},
		{'r', 1}
	};

	while(*inputStart == '-'){
		switch(*++inputStart){
			case 'w': [[fallthrough]];
			case 'r':
				++inputStart;
				skipWhitespace(inputStart);
				findWhitespace(inputStart);
				break;
			case 'e': [[fallthrough]];
			case 'p':
				++inputStart;
				break;
		}
		skipWhitespace(inputStart);
	};

	if(*inputStart == '\0'){
		println("A program to time must be present.");
		return 1;
	}

	//Parse the arguments for this program from argc/argv, as they are ASCII
	for(int i = 1; i < argc; ++i){
		if(argv[i][0] != '-'){
			break;
		}
		//Switch on the supported command line arguments
		switch(argv[i][1]){
			brase 'e':{
				if(program.echo == true){
					println("Flag -e is present multiple times.");
					return 1;
				}
				program.echo = true;
			} brase 'p':{
				if(program.portable == true){
					println("Flag -p is present multiple times.");
					return 1;
				}
				program.portable = true;
			} brase 'w':{
				if(program.warmup != 0){
					println("Flag -w is present multiple times.");
					return 1;
				}
				++i;
				auto end = argv[i] + strlen(argv[i]);
				int64 warmup{};
				std::from_chars(argv[i], end, warmup);
				if(warmup <= 0){
					println("Flag -w requires a number greater than 1.");
					return 1;
				}
				program.warmup = warmup;
			} brase 'r':{
				if(program.runs != 0){
					println("Flag -r is present multiple times.");
					return 1;
				}
				++i;
				auto end = argv[i] + strlen(argv[i]);
				int64 runs{};
				std::from_chars(argv[i], end, runs);
				if(runs <= 0){
					println("Flag -r requires a number greater than 1.");
					return 1;
				}
				program.runs = runs;
			}
		}
	}

	tstring path = inputStart;

	auto res = program.time(inputStart);
	program.printInfo();

	return res;
};