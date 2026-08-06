//
// main.cpp — classic Circle kernel entry
//
#include "kernel.h"
#include <circle/startup.h>

int main(void)
{
    CKernel Kernel;
    if (!Kernel.Initialize())
    {
        halt();
        return EXIT_HALT;
    }

    TShutdownMode ShutdownMode = Kernel.Run();

    // Every exit parks. A payload that reboots destroys the serial tail that
    // says why it exited, and takes the board away before anyone can look at
    // it — so there is no reboot path here, whatever mode Run() reports.
    (void) ShutdownMode;
    halt();
    return EXIT_HALT;
}
