#include "gui_launcher.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
extern char **environ;
#endif

namespace fs = std::filesystem;

int launch_native_gui(const char *caller) {
    fs::path origin = caller && *caller ? fs::absolute(caller).parent_path()
                                       : fs::current_path();
#ifdef _WIN32
    fs::path executable = origin / "progvoraz_gui.exe";
#else
#ifdef __APPLE__
    fs::path executable = origin / "progvoraz_gui";
    if (!fs::exists(executable)) executable = origin / "progvoraz_gui_swift";
    if (!fs::exists(executable)) executable = origin / "progvoraz_gui_portable";
#else
    fs::path executable = origin / "progvoraz_gui";
#endif
#endif
    if (!fs::is_regular_file(executable))
        throw std::runtime_error("No se encontro la GUI compilada junto al programa.");
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string absolute=executable.string();
    if (!CreateProcessA(absolute.c_str(),nullptr,nullptr,nullptr,FALSE,0,
                        nullptr,nullptr,&si,&pi))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "No se pudo iniciar la GUI Win32");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    pid_t child{};
    std::string absolute=executable.string();
    char *child_argv[]={absolute.data(),nullptr};
    int rc=posix_spawn(&child,absolute.c_str(),nullptr,nullptr,child_argv,environ);
    if (rc!=0) throw std::system_error(rc,std::generic_category(),
                                       "No se pudo iniciar la GUI");
#endif
    std::cout<<"Interfaz GUI iniciada: "<<executable.filename().string()<<'\n';
    return 0;
}
