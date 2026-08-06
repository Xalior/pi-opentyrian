//
// beacon.cpp — phase markers for a board that dies before it can speak.
//
// CKernel::Initialize reports each step it takes, but its first report comes
// after the serial device is up, and there is a whole phase before that: the
// C runtime's own start-up, the static constructors, and the construction of
// the kernel object itself. A board that fails anywhere in there produces
// nothing at all, and nothing at all is the same picture as an image that was
// never entered.
//
// So this file puts a mark on the wire at each boundary of that phase. The
// last letter to arrive names the phase that did not finish:
//
//   (nothing)  the C runtime never reached our code — start-up, the zeroing
//              of .bss, or the entry hand-off itself.
//   A          static constructors began.
//   A B        constructors finished and main() was entered.
//   A B C      the kernel object was constructed; the failure is in
//              CKernel::Initialize before the serial device came up.
//   A B C [init] ...   past this file's reach; read the step names instead.
//
// HOW IT WRITES. A serial device of its own, constructed on first use and
// driven polled. It cannot use the kernel's, because the kernel's does not
// exist yet at mark A, and it deliberately re-initialises the same port that
// CKernel will later take over — which is safe, and is the same technique the
// sibling port used to find a static constructor that was killing its boot.
//
// A function-local static rather than a global: a global here would be one
// more constructor running at an arbitrary point in the same phase this file
// exists to measure.
//
// THIS IS INSTRUMENTATION, NOT PART OF THE PRODUCT. See the README.
//
#include <circle/serial.h>

static void BeaconWrite(const char *pMsg, size_t nLen)
{
    static CSerialDevice s_Serial(0, FALSE, 0);
    static bool s_bInit = false;
    if (!s_bInit)
    {
        s_Serial.Initialize(115200);
        s_bInit = true;
    }
    s_Serial.Write(pMsg, nLen);
}

void BeaconMark(const char *pMark)
{
    size_t nLen = 0;
    while (pMark[nLen] != '\0')
        nLen++;
    BeaconWrite("<beacon ", 8);
    BeaconWrite(pMark, nLen);
    BeaconWrite(">\n", 2);
}

// Mark A: the constructor phase has begun. This file is linked first among
// this project's objects, so among our own constructors this one runs first.
// It says nothing about the library's constructors, which the archive places
// after ours — if A arrives and B does not, one of those is where it stopped.
namespace
{
struct SBeaconA
{
    SBeaconA(void) { BeaconMark("A constructors running"); }
};
SBeaconA s_BeaconA;
}
