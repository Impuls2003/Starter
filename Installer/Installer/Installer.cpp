#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "stdafx.h"
#include <string>
#include <iostream>
#include <fstream>

#include <vector>
#include <map>

#include <algorithm>
#include <comdef.h>
#include <filesystem>

#include <locale>

#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

namespace fs = std::filesystem;

using std::vector;
using std::map;
using std::wstring;
using std::wcout;
using std::endl;
using std::wifstream;

using namespace std;

typedef map<wstring, wstring> configParams;
typedef vector<configParams> configLine;
typedef map<wstring, configLine> configType;

void search(const fs::path& directory, const fs::path& file_name)
{
	auto d = fs::directory_iterator(directory);

	auto found = std::find_if(d, end(d), [&file_name](const auto& dir_entry)
	{
		return dir_entry.path().filename() == file_name;
	});
	if (found != end(d))
	{
		// we have found what we were looking for
	}

	// ...
}

BOOL Is64BitWindows()
{
#if defined(_WIN64)
	return TRUE;  // Программа скомпилирована для x64
#elif defined(_WIN32)
	// Программа скомпилирована для x32, спрашиваем ОС
	BOOL f64 = FALSE;

	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
	return FALSE; // Программа скомпилирована для x16
#endif
}

std::vector<std::wstring> splitString(std::wstring str, std::wstring delim) {
	std::vector<std::wstring> arr;
	size_t prev = 0;
	size_t next;
	size_t delta = delim.length();

	while ((next = str.find(delim, prev)) != std::wstring::npos) {
		arr.push_back(str.substr(prev, next - prev));
		prev = next + delta;
	}
	if (str.substr(prev) != L"") {
		arr.push_back(str.substr(prev));
	}
	return arr;
}

configType getConfig(const char *fname) {
	
	configType conf;

	wifstream fin(fname);			// открыли файл для чтения

	// Локаль для ввода
	locale utf8(locale(""), new codecvt_utf8<wchar_t>);
	fin.imbue(utf8);

	if (fin.is_open()) {
		while (!fin.eof()) {
			wstring param;
			getline(fin, param);
			std::vector<std::wstring> val = splitString(param, L"=");

			if (val.size() > 1) {
				std::transform(val[0].begin(), val[0].end(), val[0].begin(), ::toupper);

				configParams line;

				std::vector<std::wstring> params = splitString(val[1], L"^");

				for (vector<wstring>::iterator iter = params.begin(); iter != params.end()-1; iter++) {
					std::vector<std::wstring> valParam = splitString(*iter, L"|");
						// Если есть разделитель параметр|значение тогда записываем, иначе и параметр и значение равно параметр
					std::transform(valParam[0].begin(), valParam[0].end(), valParam[0].begin(), ::toupper);
					if (valParam.size() > 1) {
						line[valParam[0]] = valParam[1];
					} else {
						line[valParam[0]] = valParam[0];
					}
				}
				line[L"START"] = *(params.end() - 1);
				conf[val[0]].push_back(line);
			}
		}
		fin.close();				// закрываем файл
	}

	return conf;

}

void startCommand(wstring command, wstring params = L"") {
	
	//HINSTANCE hi = ShellExecute(NULL, L"open", command.c_str(), params.c_str(), NULL, SW_SHOWDEFAULT);
	if (params.length() == 0) {
		std::vector<std::wstring> val = splitString(command, L"|");
		if (val.size() > 1) {
			command = val[0];
			params	= val[1];
		}
	}

	SHELLEXECUTEINFO ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = command.c_str();
	ShExecInfo.lpParameters = params.c_str();
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow =  SW_SHOW;//SW_HIDE;
	ShExecInfo.hInstApp = NULL;
	ShellExecuteEx(&ShExecInfo);
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
	CloseHandle(ShExecInfo.hProcess);

	//_bstr_t b(command.c_str());
	//const char* c = b;
	//system(c);
}

void runConfigLine(wstring lineName, configLine line, wstring param, wstring filePath) {
	
	for (configLine::iterator iter = line.begin(); iter != line.end(); iter++) {
		configParams params = *iter;
		wstring command = L"";

		// Если в параметрах есть запуск только на 64 бит windows, а windows 32 - выходим
		if (params.find(L"X64") != params.end()) {
			if (!Is64BitWindows()) {
				continue;
			}
		}

		// Если в параметрах есть запуск только на 32 бит windows, а windows 64 - выходим
		if (params.find(L"X86") != params.end()) {
			if (Is64BitWindows()) {
				continue;
			}
		}

		// Использовать ли абсолютный путь
		if (params.find(L"ABS") != params.end()) {
			command = params[L"START"];
		} else {
			command = filePath + params[L"START"];
		}

		// Если нужно проверить существование файла
		if (params.find(L"CFEXIST") != params.end()) {
			// Если файл не найден завершаем дальнейшую работу функции
			if (!fs::exists(splitString(command, L"|")[0])) {	// Убираем из команды параметры. Оставляем только путь и имя файла
				continue;
			}
		}

		// Если нужно подождать необходимое время в секундах
		if (params.find(L"SLEEP") != params.end()) {
			std::this_thread::sleep_for(std::chrono::seconds(_wtoi(params[L"SLEEP"].c_str())));
		}

		startCommand(command, param);
	}
}

void runConfig(configType config, wstring param) {

	// Путь к исполняемому файлу
	const fs::path workdir = fs::current_path();
	wstring filePath = workdir.wstring().append(L"\\");

	// Выполнить до
	runConfigLine(L"STARTBEFORE", config[L"STARTBEFORE"], L"", filePath);

	// Выполнить сейчас
	runConfigLine(L"START", config[L"START"], param, filePath);

	// Выполнить после
	runConfigLine(L"STARTAFTER", config[L"STARTAFTER"], L"", filePath);
}

int wmain(int argc, wchar_t *argv[])
{
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);			// Скрываем окно установщика
	setlocale(LC_ALL, "Russian");							// Русская локаль	
	
	configType config = getConfig("conf.ini");				// Получаем конфигурационный файл
															// Параметры, переданные в программу
	wstring param;
	for(int i=1; i<argc; ++i){
		param.append(L" ").append(argv[i]);
	}

	runConfig(config, param);								// Запускаем конфиг на исполнение

	return 0;
}

