//
// game_paths.cpp — where OpenTyrian writes.
//
// OpenTyrian keeps two directories apart: the one it READS the Tyrian data
// files from, and the one it WRITES its configuration, its key bindings and
// its saved games to. The first is a compile-time path, TYRIAN_DIR, and the
// build sets it (see the Makefile). The second it works out at run time from
// the environment — XDG_CONFIG_HOME, or HOME, or the current directory as a
// last resort — which is the right answer on a desktop and no answer at all
// here. A bare-metal program has no environment and no shell to have given it
// a working directory.
//
// So the answer is supplied instead. get_user_directory() is redirected with
// the linker's --wrap, the same mechanism the file syscalls and the surface
// layer use, and returns the same directory the data files live in. Saved
// games then sit beside the game on the card, which is where someone looking
// at the card would expect to find them, and nothing in the game's own source
// is touched.
//
// The directory has to exist on the card. It does: it is the directory the
// data files were copied into.
//
extern "C" {

const char *__real_get_user_directory(void);

const char *__wrap_get_user_directory(void)
{
    return TYRIAN_DIR;
}

} // extern "C"
