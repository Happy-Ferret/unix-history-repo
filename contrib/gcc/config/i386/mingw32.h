/* Operating system specific defines to be used when targeting GCC for
   hosting on Windows32, using GNU tools and the Windows32 API Library.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Most of this is the same as for cygwin, except for changing some
   specs.  */

/* Mingw GCC, unlike Cygwin's, must be relocatable. This macro must 
   be defined before any other files are included.  */
#ifndef WIN32_NO_ABSOLUTE_INST_DIRS
#define WIN32_NO_ABSOLUTE_INST_DIRS 1
#endif

#include "i386/cygwin.h"

#define TARGET_EXECUTABLE_SUFFIX ".exe"

/* See i386/crtdll.h for an altervative definition.  */
#define EXTRA_OS_CPP_BUILTINS()					\
  do								\
    {								\
      builtin_define ("__MSVCRT__");				\
      builtin_define ("__MINGW32__");			   	\
    }								\
  while (0)

#undef TARGET_OS_CPP_BUILTINS	/* From cygwin.h.  */
#define TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
	builtin_define ("_WIN32");					\
	builtin_define_std ("WIN32");					\
	builtin_define_std ("WINNT");					\
	builtin_define ("_X86_=1");					\
	builtin_define ("__stdcall=__attribute__((__stdcall__))");	\
	builtin_define ("__cdecl=__attribute__((__cdecl__))");		\
	builtin_define ("__declspec(x)=__attribute__((x))");		\
	if (!flag_iso)							\
	  {								\
	    builtin_define ("_stdcall=__attribute__((__stdcall__))");	\
	    builtin_define ("_cdecl=__attribute__((__cdecl__))");	\
	  }								\
	EXTRA_OS_CPP_BUILTINS ();					\
	builtin_assert ("system=winnt");				\
    }									\
  while (0)

/* Specific a different directory for the standard include files.  */
#undef STANDARD_INCLUDE_DIR
#define STANDARD_INCLUDE_DIR "/usr/local/mingw32/include"
#undef STANDARD_INCLUDE_COMPONENT
#define STANDARD_INCLUDE_COMPONENT "MINGW"

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{mthreads:-D_MT}"

/* For Windows applications, include more libraries, but always include
   kernel32.  */
#undef LIB_SPEC
#define LIB_SPEC "%{pg:-lgmon} %{mwindows:-lgdi32 -lcomdlg32} \
                  -luser32 -lkernel32 -ladvapi32 -lshell32"

/* Include in the mingw32 libraries with libgcc */
#undef LINK_SPEC
#define LINK_SPEC "%{mwindows:--subsystem windows} \
  %{mconsole:--subsystem console} \
  %{shared: %{mdll: %eshared and mdll are not compatible}} \
  %{shared: --shared} %{mdll:--dll} \
  %{static:-Bstatic} %{!static:-Bdynamic} \
  %{shared|mdll: -e _DllMainCRTStartup@12}"

/* Include in the mingw32 libraries with libgcc */
#undef LIBGCC_SPEC
#define LIBGCC_SPEC \
  "%{mthreads:-lmingwthrd} -lmingw32 -lgcc -lmoldname -lmingwex -lmsvcrt"

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{shared|mdll:dllcrt2%O%s} \
  %{!shared:%{!mdll:crt2%O%s}} %{pg:gcrt2%O%s}"

/* MS runtime does not need a separate math library.  */
#undef MATH_LIBRARY
#define MATH_LIBRARY ""

/* Output STRING, a string representing a filename, to FILE.
   We canonicalize it to be in Unix format (backslashe are replaced
   forward slashes.  */
#undef OUTPUT_QUOTED_STRING
#define OUTPUT_QUOTED_STRING(FILE, STRING)               \
do {						         \
  char c;					         \
						         \
  putc ('\"', asm_file);			         \
						         \
  while ((c = *string++) != 0)			         \
    {						         \
      if (c == '\\')				         \
	c = '/';				         \
						         \
      if (ISPRINT (c))                                   \
        {                                                \
          if (c == '\"')			         \
	    putc ('\\', asm_file);		         \
          putc (c, asm_file);			         \
        }                                                \
      else                                               \
        fprintf (asm_file, "\\%03o", (unsigned char) c); \
    }						         \
						         \
  putc ('\"', asm_file);			         \
} while (0)

/* Define as short unsigned for compatability with MS runtime.  */
#undef WINT_TYPE
#define WINT_TYPE "short unsigned int"
