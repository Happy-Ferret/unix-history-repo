# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining MK_WARNS=no.

# for GCC:   http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Warning-Options.html

.include <bsd.compiler.mk>

# the default is gnu99 for now
CSTD?=		gnu99

.if ${CSTD} == "k&r"
CFLAGS+=	-traditional
.elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS+=	-std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS+=	-std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS+=	-std=iso9899:1999
.else # CSTD
CFLAGS+=	-std=${CSTD}
.endif # CSTD
# -pedantic is problematic because it also imposes namespace restrictions
#CFLAGS+=	-pedantic
.if defined(WARNS)
.if ${WARNS} >= 1
CWARNFLAGS+=	-Wsystem-headers
.if !defined(NO_WERROR) && !defined(NO_WERROR.${COMPILER_TYPE})
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && !NO_WERROR.${COMPILER_TYPE}
.endif # WARNS >= 1
.if ${WARNS} >= 2
CWARNFLAGS+=	-Wall -Wno-format-y2k
.endif # WARNS >= 2
.if ${WARNS} >= 3
CWARNFLAGS+=	-W -Wno-unused-parameter -Wstrict-prototypes\
		-Wmissing-prototypes -Wpointer-arith
.endif # WARNS >= 3
.if ${WARNS} >= 4
CWARNFLAGS+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow\
		-Wunused-parameter
.if !defined(NO_WCAST_ALIGN) && !defined(NO_WCAST_ALIGN.${COMPILER_TYPE})
CWARNFLAGS+=	-Wcast-align
.endif # !NO_WCAST_ALIGN !NO_WCAST_ALIGN.${COMPILER_TYPE}
.endif # WARNS >= 4
# BDECFLAGS
.if ${WARNS} >= 6
CWARNFLAGS+=	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls\
		-Wold-style-definition
.if !defined(NO_WMISSING_VARIABLE_DECLARATIONS)
CWARNFLAGS.clang+=	-Wmissing-variable-declarations
.endif
.endif # WARNS >= 6
.if ${WARNS} >= 2 && ${WARNS} <= 4
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS+=	-Wno-uninitialized
.endif # WARNS >=2 && WARNS <= 4
CWARNFLAGS+=	-Wno-pointer-sign
# Clang has more warnings enabled by default, and when using -Wall, so if WARNS
# is set to low values, these have to be disabled explicitly.
.if ${WARNS} <= 6
CWARNFLAGS.clang+=	-Wno-empty-body -Wno-string-plus-int
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} > 30300
CWARNFLAGS.clang+= -Wno-unused-const-variable
.endif
.endif # WARNS <= 6
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-unused-value\
		-Wno-parentheses-equality -Wno-unused-function -Wno-enum-conversion
.endif # WARNS <= 3
.if ${WARNS} <= 2
CWARNFLAGS.clang+=	-Wno-switch -Wno-switch-enum -Wno-knr-promoted-parameter
.endif # WARNS <= 2
.if ${WARNS} <= 1
CWARNFLAGS.clang+=	-Wno-parentheses
.endif # WARNS <= 1
.if defined(NO_WARRAY_BOUNDS)
CWARNFLAGS.clang+=	-Wno-array-bounds
.endif # NO_WARRAY_BOUNDS
.endif # WARNS

.if defined(FORMAT_AUDIT)
WFORMAT=	1
.endif # FORMAT_AUDIT
.if defined(WFORMAT)
.if ${WFORMAT} > 0
#CWARNFLAGS+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS+=	-Wformat=2 -Wno-format-extra-args
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-format-nonliteral
.endif # WARNS <= 3
.if !defined(NO_WERROR) && !defined(NO_WERROR.${COMPILER_TYPE})
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && !NO_WERROR.${COMPILER_TYPE}
.endif # WFORMAT > 0
.endif # WFORMAT
.if defined(NO_WFORMAT) || defined(NO_WFORMAT.${COMPILER_TYPE})
CWARNFLAGS+=	-Wno-format
.endif # NO_WFORMAT || NO_WFORMAT.${COMPILER_TYPE}

.if defined(IGNORE_PRAGMA)
CWARNFLAGS+=	-Wno-unknown-pragmas
.endif # IGNORE_PRAGMA

.if ${COMPILER_TYPE} == "clang"
# Would love to do this unconditionally, but can't due to its use in
# kernel build coupled with CFLAGS.${TARGET} feature
CLANG_NO_IAS=	 -no-integrated-as
.endif
CLANG_OPT_SMALL= -mstack-alignment=8 -mllvm -inline-threshold=3\
		 -mllvm -enable-load-pre=false -mllvm -simplifycfg-dup-ret
CFLAGS.clang+=	 -Qunused-arguments
.if ${MACHINE_CPUARCH} == "sparc64"
# Don't emit .cfi directives, since we must use GNU as on sparc64, for now.
CFLAGS.clang+=	 -fno-dwarf2-cfi-asm
.endif # SPARC64
# The libc++ headers use c++11 extensions.  These are normally silenced because
# they are treated as system headers, but we explicitly disable that warning
# suppression when building the base system to catch bugs in our headers.
# Eventually we'll want to start building the base system C++ code as C++11,
# but not yet.
CXXFLAGS.clang+=	 -Wno-c++11-extensions

.if ${MK_SSP} != "no" && ${MACHINE_CPUARCH} != "ia64" && \
    ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips"
# Don't use -Wstack-protector as it breaks world with -Werror.
SSP_CFLAGS?=	-fstack-protector
CFLAGS+=	${SSP_CFLAGS}
.endif # SSP && !IA64 && !ARM && !MIPS

# Allow user-specified additional warning flags, plus compiler specific flag overrides.
# Unless we've overriden this...
.if ${MK_WARNS} != "no"
CFLAGS+=	${CWARNFLAGS} ${CWARNFLAGS.${COMPILER_TYPE}}
.endif

CFLAGS+=	 ${CFLAGS.${COMPILER_TYPE}}
CXXFLAGS+=	 ${CXXFLAGS.${COMPILER_TYPE}}

# Tell bmake not to mistake standard targets for things to be searched for
# or expect to ever be up-to-date.
PHONY_NOTMAIN = afterdepend afterinstall all beforedepend beforeinstall \
		beforelinking build build-tools buildfiles buildincludes \
		checkdpadd clean cleandepend cleandir cleanobj configure \
		depend dependall distclean distribute exe \
		html includes install installfiles installincludes lint \
		obj objlink objs objwarn realall realdepend \
		realinstall regress subdir-all subdir-depend subdir-install \
		tags whereobj

.PHONY: ${PHONY_NOTMAIN}
.NOTMAIN: ${PHONY_NOTMAIN}
