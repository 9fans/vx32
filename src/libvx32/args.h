/*

Simple command-line argument parser from Plan 9's u9fs:

The authors of this software are Bob Flandrena, Ken Thompson,
Rob Pike, and Russ Cox.

		Copyright (c) 1992-2002 by Lucent Technologies.

Permission to use, copy, modify, and distribute this software for any
purpose without fee is hereby granted, provided that this entire notice
is included in all copies of any software which is or includes a copy
or modification of this software and in all copies of the supporting
documentation for such software.

THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

     SYNOPSIS
          #include "args.h"

          ARGBEGIN {
          char *ARGF();
          char *EARGF(code);
          Rune ARGC();
          } ARGEND

          extern char *argv0;

     DESCRIPTION
          These macros assume the names argc and argv are in scope.
          ARGBEGIN and ARGEND surround code for processing program
          options.  The code should be the cases of a C switch on option
          characters; it is executed once for each option character.
          Options end after an argument --, before an argument -, or
          before an argument that doesn't begin with -.

          The function macro ARGC returns the current option charac-
          ter, as an integer.

          The function macro ARGF returns the current option argument:
          a pointer to the rest of the option string if not empty, or
          the next argument in argv if any, or 0.  ARGF must be called
          just once for each option that takes an argument.  The macro
          EARGF is like ARGF but instead of returning zero runs code
          and, if that returns, calls abort(). A typical value for
          code is usage(), as in EARGF(usage()).

          After ARGBEGIN, argv0 is a copy of argv[0] (conventionally
          the name of the program).

          After ARGEND, argv points at a zero-terminated list of the
          remaining argc arguments.

     EXAMPLE
          This C program can take option b and option f, which
          requires an argument.

               void
               main(int argc, char *argv[])
               {
                       char *f;
                       print("%s", argv[0]);
                       ARGBEGIN {
                       case 'b':
                               print(" -b");
                               break;
                       case 'f':
                               print(" -f(%s)", (f=ARGF())? f: "no arg");
                               break;
                       default:
                               print(" badflag('%c')", ARGC());
                       } ARGEND
                       print(" %d args:", argc);
                       while(*argv)
                               print(" '%s'", *argv++);
                       print("\n");
                       exits(nil);
               }

          Here is the output from running the command 
          "prog -bffile1 -r -f file2 arg1 arg2":

               prog -b -f(file1) badflag('r') -f(file2) 2 args: 'arg1' 'arg2'

 */

extern const char	*argv0;

#define _ARGSET(x)	(x) = 0
#define _ARGUSED(x)	if (x) { } else

#define	ARGBEGIN	for((argv?0:(argv=(void*)&argc)),(argv0?0:(argv0=*argv)),\
			    argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				const char *_args;\
				const char *_argt;\
				char _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(*_args && (_argc = *_args++))\
				switch(_argc)
#define	ARGEND		_ARGSET(_argt);_ARGUSED(_argt);_ARGUSED(_argc);_ARGUSED(_args);}_ARGUSED(argv);_ARGUSED(argc);
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	EARGF(x)	(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): ((x), abort(), (char*)0)))

#define	ARGC()		_argc
