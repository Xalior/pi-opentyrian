//
// stub_opentyrian.cpp — boot-bisection payload.
//
// Links in place of every OpenTyrian object (`make STUB=1`) so the image is
// the host scaffolding alone: the same kernel wrapper, the same world, the
// same link recipe, none of the game's code.
//
// It answers one question, and it is the first question worth asking when a
// board boots and says nothing at all:
//
//   the stub image LOGS   — the scaffolding, the world and the link are
//                           sound, and the fault is in the game or in the
//                           layer between it and the library.
//   the stub image is SILENT — the fault is in the scaffolding, the world or
//                           the build, and none of the game's code is
//                           involved.
//
// Without it, a silent board leaves both halves suspected at once and there
// is nothing to measure.
//
// OpenTyrian is C and defines no global objects that other translation units
// reach for, so unlike the equivalent stub in pi-cannonball this file has no
// globals to stand in for: taking the game's objects out of the link takes
// out everything that referenced them too.
//
// THIS IS INSTRUMENTATION, NOT PART OF THE PRODUCT. It is built only when
// asked for, it is never what `make kernels` produces, and it is recorded in
// the README as something to remove once the port is proven on hardware.
//
#include <circle/logger.h>

extern "C" int opentyrian_main(int argc, char *argv[])
{
    CLogger::Get()->Write("stub", LogNotice,
                          "stub payload reached: host scaffolding boots");
    CLogger::Get()->Write("stub", LogNotice, "argc = %d", argc);
    for (int i = 0; i < argc; i++)
        CLogger::Get()->Write("stub", LogNotice, "  argv[%d] = %s", i,
                              argv[i] != nullptr ? argv[i] : "(null)");

    // Returning hands control back to the kernel, which parks and says so.
    return 0;
}
