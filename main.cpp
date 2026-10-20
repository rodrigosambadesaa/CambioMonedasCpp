#include "app_console.h"
#include "portable_cli.h"

// Only argument dispatch belongs in main. Interactive workflows are
// implemented independently in app_console.cpp, as in the C reference.
int main(int argc, char **argv)
{
    if (argc > 1)
    {
        int exitCode = run_portable_cli(argc, argv);
        if (exitCode != -1)
            return exitCode;
    }
    return app_console_run();
}
