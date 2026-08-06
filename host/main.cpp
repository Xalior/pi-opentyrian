//
// main.cpp — Circle kernel entry.
//
// NOTHING HERE REBOOTS. A board that reboots on failure destroys the two
// things worth having: the tail of the serial log, which is cut off mid-line
// while the UART is still draining, and the machine state someone was about
// to inspect. So every path ends parked — the board sits still and says why,
// and a power cycle is what starts it again.
//
// This matters most for the failures that happen before there is anything to
// see. If Initialize() gives up, the reason is already on the serial port
// (kernel.cpp reports each step as it passes), and this file's job is only to
// stop without erasing it.
//
#include "kernel.h"
#include <circle/startup.h>

// Boot phase markers (beacon.cpp). Instrumentation; see the README.
void BeaconMark(const char *pMark);

static void Park(void)
{
    for (;;)
    {
        asm volatile("wfe" ::: "memory");
    }
}

int main(void)
{
    BeaconMark("B main entered");

    CKernel Kernel;

    BeaconMark("C kernel constructed");

    if (!Kernel.Initialize())
    {
        // Initialize() has already said which step failed, on the serial
        // port, by writing to it directly — it cannot rely on the logger,
        // because the logger is one of the things that may not have come up.
        Park();
    }

    Kernel.Run();

    // Run() parks on its own once the game returns. Reaching here at all
    // means it returned unexpectedly, so park rather than fall off the end
    // of main into whatever follows.
    Park();

    return EXIT_HALT;   // never reached
}
