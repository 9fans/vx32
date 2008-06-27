// Figure out whether to use %fs or %gs as the vx32 control segment.
// The host operating system is likely to use one of them as the 
// thread-local storage segment.  Use the other one.

#if defined(__APPLE__)
#	define EXT(s) _ ## s
#	define VSEG_FS
#elif defined(__FreeBSD__)
#	define VSEG_FS
#elif defined(__linux__)
#	if defined(__i386__)
#		define VSEG_FS
#	else /* x86-64 */
#		define VSEG_GS
#	endif
#else
#error	Unsupported operating system.
#endif

#ifdef VSEG_FS
#	define VSEG %fs
#	define VSEGSTR "%fs"
#	define VSEGPREFIX 0x64
#	define mc_vs mc_fs
#	define ss_vs ss.fs
#else
#	define VSEG  %gs
#	define VSEGSTR  "%gs"
#	define VSEGPREFIX 0x65
#	define mc_vs mc_gs
#	define ss_vs ss.gs
#endif

#ifndef EXT
#	define EXT(s) s
#endif
