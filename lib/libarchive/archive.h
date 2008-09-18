/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef ARCHIVE_H_INCLUDED
#define	ARCHIVE_H_INCLUDED

/*
 * Note: archive.h is for use outside of libarchive; the configuration
 * headers (config.h, archive_platform.h, etc.) are purely internal.
 * Do NOT use HAVE_XXX configuration macros to control the behavior of
 * this header!  If you must conditionalize, use predefined compiler and/or
 * platform macros.
 */

#include <sys/types.h>  /* Linux requires this for off_t */
#if !defined(__WATCOMC__) && !defined(_MSC_VER)
/* Header unavailable on Watcom C or MS Visual C++. */
#include <inttypes.h> /* int64_t, etc. */
#endif
#include <stdio.h> /* For FILE * */

/* Get appropriate definitions of standard POSIX-style types. */
/* These should match the types used in 'struct stat' */
#ifdef _WIN32
#define	__LA_SSIZE_T	long
#define	__LA_UID_T	unsigned int
#define	__LA_GID_T	unsigned int
#else
#include <unistd.h>  /* ssize_t, uid_t, and gid_t */
#define	__LA_SSIZE_T	ssize_t
#define	__LA_UID_T	uid_t
#define	__LA_GID_T	gid_t
#endif

/*
 * On Windows, define LIBARCHIVE_STATIC if you're building or using a
 * .lib.  The default here assumes you're building a DLL.  Only
 * libarchive source should ever define __LIBARCHIVE_BUILD.
 */
#if ((defined __WIN32__) || (defined _WIN32)) && (!defined LIBARCHIVE_STATIC)
# ifdef __LIBARCHIVE_BUILD
#  ifdef __GNUC__
#   define __LA_DECL	__attribute__((dllexport)) extern
#  else
#   define __LA_DECL	__declspec(dllexport)
#  endif
# else
#  ifdef __GNUC__
#   define __LA_DECL	__attribute__((dllimport)) extern
#  else
#   define __LA_DECL	__declspec(dllimport)
#  endif
# endif
#else
/* Static libraries or non-Windows needs no special declaration. */
# define __LA_DECL
#endif


#ifdef __cplusplus
extern "C" {
#endif

/*
 * The version number is provided as both a macro and a function.
 * The macro identifies the installed header; the function identifies
 * the library version (which may not be the same if you're using a
 * dynamically-linked version of the library).  Of course, if the
 * header and library are very different, you should expect some
 * strangeness.  Don't do that.
 */

/*
 * The version number is expressed as a single integer that makes it
 * easy to compare versions at build time: for version a.b.c, the
 * version number is printf("%d%03d%03d",a,b,c).  For example, if you
 * know your application requires version 2.12.108 or later, you can
 * assert that ARCHIVE_VERSION >= 2012108.
 *
 * This single-number format was introduced with libarchive 1.9.0 in
 * the libarchive 1.x family and libarchive 2.2.4 in the libarchive
 * 2.x family.  The following may be useful if you really want to do
 * feature detection for earlier libarchive versions (which defined
 * ARCHIVE_API_VERSION and ARCHIVE_API_FEATURE instead):
 *
 * #ifndef ARCHIVE_VERSION_NUMBER
 * #define ARCHIVE_VERSION_NUMBER	\
 *             (ARCHIVE_API_VERSION * 1000000 + ARCHIVE_API_FEATURE * 1000)
 * #endif
 */
#define	ARCHIVE_VERSION_NUMBER 2005005
__LA_DECL int		archive_version_number(void);

/*
 * Textual name/version of the library, useful for version displays.
 */
#define	ARCHIVE_VERSION_STRING "libarchive 2.5.5"
__LA_DECL const char *	archive_version_string(void);

#if ARCHIVE_VERSION_NUMBER < 3000000
/*
 * Deprecated; these are older names that will be removed in favor of
 * the simpler definitions above.
 */
#define	ARCHIVE_VERSION_STAMP	ARCHIVE_VERSION_NUMBER
__LA_DECL int		archive_version_stamp(void);
#define	ARCHIVE_LIBRARY_VERSION	ARCHIVE_VERSION_STRING
__LA_DECL const char *	archive_version(void);
#define	ARCHIVE_API_VERSION	(ARCHIVE_VERSION_NUMBER / 1000000)
__LA_DECL int		archive_api_version(void);
#define	ARCHIVE_API_FEATURE	((ARCHIVE_VERSION_NUMBER / 1000) % 1000)
__LA_DECL int		archive_api_feature(void);
#endif

#if ARCHIVE_VERSION_NUMBER < 3000000
/* This should never have been here in the first place. */
/* Legacy of old tar assumptions, will be removed in libarchive 3.0. */
#define	ARCHIVE_BYTES_PER_RECORD	  512
#define	ARCHIVE_DEFAULT_BYTES_PER_BLOCK	10240
#endif

/* Declare our basic types. */
struct archive;
struct archive_entry;

/*
 * Error codes: Use archive_errno() and archive_error_string()
 * to retrieve details.  Unless specified otherwise, all functions
 * that return 'int' use these codes.
 */
#define	ARCHIVE_EOF	  1	/* Found end of archive. */
#define	ARCHIVE_OK	  0	/* Operation was successful. */
#define	ARCHIVE_RETRY	(-10)	/* Retry might succeed. */
#define	ARCHIVE_WARN	(-20)	/* Partial success. */
/* For example, if write_header "fails", then you can't push data. */
#define	ARCHIVE_FAILED	(-25)	/* Current operation cannot complete. */
/* But if write_header is "fatal," then this archive is dead and useless. */
#define	ARCHIVE_FATAL	(-30)	/* No more operations are possible. */

/*
 * As far as possible, archive_errno returns standard platform errno codes.
 * Of course, the details vary by platform, so the actual definitions
 * here are stored in "archive_platform.h".  The symbols are listed here
 * for reference; as a rule, clients should not need to know the exact
 * platform-dependent error code.
 */
/* Unrecognized or invalid file format. */
/* #define	ARCHIVE_ERRNO_FILE_FORMAT */
/* Illegal usage of the library. */
/* #define	ARCHIVE_ERRNO_PROGRAMMER_ERROR */
/* Unknown or unclassified error. */
/* #define	ARCHIVE_ERRNO_MISC */

/*
 * Callbacks are invoked to automatically read/skip/write/open/close the
 * archive. You can provide your own for complex tasks (like breaking
 * archives across multiple tapes) or use standard ones built into the
 * library.
 */

/* Returns pointer and size of next block of data from archive. */
typedef __LA_SSIZE_T	archive_read_callback(struct archive *, void *_client_data,
		    const void **_buffer);
/* Skips at most request bytes from archive and returns the skipped amount */
#if ARCHIVE_VERSION_NUMBER < 2000000
typedef __LA_SSIZE_T	archive_skip_callback(struct archive *, void *_client_data,
		    size_t request);
#else
typedef off_t	archive_skip_callback(struct archive *, void *_client_data,
		    off_t request);
#endif
/* Returns size actually written, zero on EOF, -1 on error. */
typedef __LA_SSIZE_T	archive_write_callback(struct archive *, void *_client_data,
		    const void *_buffer, size_t _length);
typedef int	archive_open_callback(struct archive *, void *_client_data);
typedef int	archive_close_callback(struct archive *, void *_client_data);

/*
 * Codes for archive_compression.
 */
#define	ARCHIVE_COMPRESSION_NONE	0
#define	ARCHIVE_COMPRESSION_GZIP	1
#define	ARCHIVE_COMPRESSION_BZIP2	2
#define	ARCHIVE_COMPRESSION_COMPRESS	3
#define	ARCHIVE_COMPRESSION_PROGRAM	4

/*
 * Codes returned by archive_format.
 *
 * Top 16 bits identifies the format family (e.g., "tar"); lower
 * 16 bits indicate the variant.  This is updated by read_next_header.
 * Note that the lower 16 bits will often vary from entry to entry.
 * In some cases, this variation occurs as libarchive learns more about
 * the archive (for example, later entries might utilize extensions that
 * weren't necessary earlier in the archive; in this case, libarchive
 * will change the format code to indicate the extended format that
 * was used).  In other cases, it's because different tools have
 * modified the archive and so different parts of the archive
 * actually have slightly different formts.  (Both tar and cpio store
 * format codes in each entry, so it is quite possible for each
 * entry to be in a different format.)
 */
#define	ARCHIVE_FORMAT_BASE_MASK		0xff0000
#define	ARCHIVE_FORMAT_CPIO			0x10000
#define	ARCHIVE_FORMAT_CPIO_POSIX		(ARCHIVE_FORMAT_CPIO | 1)
#define	ARCHIVE_FORMAT_CPIO_BIN_LE		(ARCHIVE_FORMAT_CPIO | 2)
#define	ARCHIVE_FORMAT_CPIO_BIN_BE		(ARCHIVE_FORMAT_CPIO | 3)
#define	ARCHIVE_FORMAT_CPIO_SVR4_NOCRC		(ARCHIVE_FORMAT_CPIO | 4)
#define	ARCHIVE_FORMAT_CPIO_SVR4_CRC		(ARCHIVE_FORMAT_CPIO | 5)
#define	ARCHIVE_FORMAT_SHAR			0x20000
#define	ARCHIVE_FORMAT_SHAR_BASE		(ARCHIVE_FORMAT_SHAR | 1)
#define	ARCHIVE_FORMAT_SHAR_DUMP		(ARCHIVE_FORMAT_SHAR | 2)
#define	ARCHIVE_FORMAT_TAR			0x30000
#define	ARCHIVE_FORMAT_TAR_USTAR		(ARCHIVE_FORMAT_TAR | 1)
#define	ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE	(ARCHIVE_FORMAT_TAR | 2)
#define	ARCHIVE_FORMAT_TAR_PAX_RESTRICTED	(ARCHIVE_FORMAT_TAR | 3)
#define	ARCHIVE_FORMAT_TAR_GNUTAR		(ARCHIVE_FORMAT_TAR | 4)
#define	ARCHIVE_FORMAT_ISO9660			0x40000
#define	ARCHIVE_FORMAT_ISO9660_ROCKRIDGE	(ARCHIVE_FORMAT_ISO9660 | 1)
#define	ARCHIVE_FORMAT_ZIP			0x50000
#define	ARCHIVE_FORMAT_EMPTY			0x60000
#define	ARCHIVE_FORMAT_AR			0x70000
#define	ARCHIVE_FORMAT_AR_GNU			(ARCHIVE_FORMAT_AR | 1)
#define	ARCHIVE_FORMAT_AR_BSD			(ARCHIVE_FORMAT_AR | 2)
#define	ARCHIVE_FORMAT_MTREE			0x80000

/*-
 * Basic outline for reading an archive:
 *   1) Ask archive_read_new for an archive reader object.
 *   2) Update any global properties as appropriate.
 *      In particular, you'll certainly want to call appropriate
 *      archive_read_support_XXX functions.
 *   3) Call archive_read_open_XXX to open the archive
 *   4) Repeatedly call archive_read_next_header to get information about
 *      successive archive entries.  Call archive_read_data to extract
 *      data for entries of interest.
 *   5) Call archive_read_finish to end processing.
 */
__LA_DECL struct archive	*archive_read_new(void);

/*
 * The archive_read_support_XXX calls enable auto-detect for this
 * archive handle.  They also link in the necessary support code.
 * For example, if you don't want bzlib linked in, don't invoke
 * support_compression_bzip2().  The "all" functions provide the
 * obvious shorthand.
 */
__LA_DECL int		 archive_read_support_compression_all(struct archive *);
__LA_DECL int		 archive_read_support_compression_bzip2(struct archive *);
__LA_DECL int		 archive_read_support_compression_compress(struct archive *);
__LA_DECL int		 archive_read_support_compression_gzip(struct archive *);
__LA_DECL int		 archive_read_support_compression_none(struct archive *);
__LA_DECL int		 archive_read_support_compression_program(struct archive *,
		     const char *command);

__LA_DECL int		 archive_read_support_format_all(struct archive *);
__LA_DECL int		 archive_read_support_format_ar(struct archive *);
__LA_DECL int		 archive_read_support_format_cpio(struct archive *);
__LA_DECL int		 archive_read_support_format_empty(struct archive *);
__LA_DECL int		 archive_read_support_format_gnutar(struct archive *);
__LA_DECL int		 archive_read_support_format_iso9660(struct archive *);
__LA_DECL int		 archive_read_support_format_mtree(struct archive *);
__LA_DECL int		 archive_read_support_format_tar(struct archive *);
__LA_DECL int		 archive_read_support_format_zip(struct archive *);


/* Open the archive using callbacks for archive I/O. */
__LA_DECL int		 archive_read_open(struct archive *, void *_client_data,
		     archive_open_callback *, archive_read_callback *,
		     archive_close_callback *);
__LA_DECL int		 archive_read_open2(struct archive *, void *_client_data,
		     archive_open_callback *, archive_read_callback *,
		     archive_skip_callback *, archive_close_callback *);

/*
 * A variety of shortcuts that invoke archive_read_open() with
 * canned callbacks suitable for common situations.  The ones that
 * accept a block size handle tape blocking correctly.
 */
/* Use this if you know the filename.  Note: NULL indicates stdin. */
__LA_DECL int		 archive_read_open_filename(struct archive *,
		     const char *_filename, size_t _block_size);
/* archive_read_open_file() is a deprecated synonym for ..._open_filename(). */
__LA_DECL int		 archive_read_open_file(struct archive *,
		     const char *_filename, size_t _block_size);
/* Read an archive that's stored in memory. */
__LA_DECL int		 archive_read_open_memory(struct archive *,
		     void * buff, size_t size);
/* A more involved version that is only used for internal testing. */
__LA_DECL int		archive_read_open_memory2(struct archive *a, void *buff,
		     size_t size, size_t read_size);
/* Read an archive that's already open, using the file descriptor. */
__LA_DECL int		 archive_read_open_fd(struct archive *, int _fd,
		     size_t _block_size);
/* Read an archive that's already open, using a FILE *. */
/* Note: DO NOT use this with tape drives. */
__LA_DECL int		 archive_read_open_FILE(struct archive *, FILE *_file);

/* Parses and returns next entry header. */
__LA_DECL int		 archive_read_next_header(struct archive *,
		     struct archive_entry **);

/*
 * Retrieve the byte offset in UNCOMPRESSED data where last-read
 * header started.
 */
__LA_DECL int64_t		 archive_read_header_position(struct archive *);

/* Read data from the body of an entry.  Similar to read(2). */
__LA_DECL __LA_SSIZE_T		 archive_read_data(struct archive *, void *, size_t);
/*
 * A zero-copy version of archive_read_data that also exposes the file offset
 * of each returned block.  Note that the client has no way to specify
 * the desired size of the block.  The API does guarantee that offsets will
 * be strictly increasing and that returned blocks will not overlap.
 */
__LA_DECL int		 archive_read_data_block(struct archive *a,
		    const void **buff, size_t *size, off_t *offset);

/*-
 * Some convenience functions that are built on archive_read_data:
 *  'skip': skips entire entry
 *  'into_buffer': writes data into memory buffer that you provide
 *  'into_fd': writes data to specified filedes
 */
__LA_DECL int		 archive_read_data_skip(struct archive *);
__LA_DECL int		 archive_read_data_into_buffer(struct archive *, void *buffer,
		     __LA_SSIZE_T len);
__LA_DECL int		 archive_read_data_into_fd(struct archive *, int fd);

/*-
 * Convenience function to recreate the current entry (whose header
 * has just been read) on disk.
 *
 * This does quite a bit more than just copy data to disk. It also:
 *  - Creates intermediate directories as required.
 *  - Manages directory permissions:  non-writable directories will
 *    be initially created with write permission enabled; when the
 *    archive is closed, dir permissions are edited to the values specified
 *    in the archive.
 *  - Checks hardlinks:  hardlinks will not be extracted unless the
 *    linked-to file was also extracted within the same session. (TODO)
 */

/* The "flags" argument selects optional behavior, 'OR' the flags you want. */

/* Default: Do not try to set owner/group. */
#define	ARCHIVE_EXTRACT_OWNER			(0x0001)
/* Default: Do obey umask, do not restore SUID/SGID/SVTX bits. */
#define	ARCHIVE_EXTRACT_PERM			(0x0002)
/* Default: Do not restore mtime/atime. */
#define	ARCHIVE_EXTRACT_TIME			(0x0004)
/* Default: Replace existing files. */
#define	ARCHIVE_EXTRACT_NO_OVERWRITE 		(0x0008)
/* Default: Try create first, unlink only if create fails with EEXIST. */
#define	ARCHIVE_EXTRACT_UNLINK			(0x0010)
/* Default: Do not restore ACLs. */
#define	ARCHIVE_EXTRACT_ACL			(0x0020)
/* Default: Do not restore fflags. */
#define	ARCHIVE_EXTRACT_FFLAGS			(0x0040)
/* Default: Do not restore xattrs. */
#define	ARCHIVE_EXTRACT_XATTR 			(0x0080)
/* Default: Do not try to guard against extracts redirected by symlinks. */
/* Note: With ARCHIVE_EXTRACT_UNLINK, will remove any intermediate symlink. */
#define	ARCHIVE_EXTRACT_SECURE_SYMLINKS		(0x0100)
/* Default: Do not reject entries with '..' as path elements. */
#define	ARCHIVE_EXTRACT_SECURE_NODOTDOT		(0x0200)
/* Default: Create parent directories as needed. */
#define	ARCHIVE_EXTRACT_NO_AUTODIR		(0x0400)
/* Default: Overwrite files, even if one on disk is newer. */
#define	ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER	(0x0800)
/* Detect blocks of 0 and write holes instead. */
#define	ARCHIVE_EXTRACT_SPARSE			(0x1000)

__LA_DECL int	 archive_read_extract(struct archive *, struct archive_entry *,
		     int flags);
__LA_DECL int	 archive_read_extract2(struct archive *, struct archive_entry *,
		     struct archive * /* dest */);
__LA_DECL void	 archive_read_extract_set_progress_callback(struct archive *,
		     void (*_progress_func)(void *), void *_user_data);

/* Record the dev/ino of a file that will not be written.  This is
 * generally set to the dev/ino of the archive being read. */
__LA_DECL void		archive_read_extract_set_skip_file(struct archive *,
		     dev_t, ino_t);

/* Close the file and release most resources. */
__LA_DECL int		 archive_read_close(struct archive *);
/* Release all resources and destroy the object. */
/* Note that archive_read_finish will call archive_read_close for you. */
#if ARCHIVE_VERSION_NUMBER >= 2000000
__LA_DECL int		 archive_read_finish(struct archive *);
#else
/* Temporarily allow library to compile with either 1.x or 2.0 API. */
/* Erroneously declared to return void in libarchive 1.x */
__LA_DECL void		 archive_read_finish(struct archive *);
#endif

/*-
 * To create an archive:
 *   1) Ask archive_write_new for a archive writer object.
 *   2) Set any global properties.  In particular, you should set
 *      the compression and format to use.
 *   3) Call archive_write_open to open the file (most people
 *       will use archive_write_open_file or archive_write_open_fd,
 *       which provide convenient canned I/O callbacks for you).
 *   4) For each entry:
 *      - construct an appropriate struct archive_entry structure
 *      - archive_write_header to write the header
 *      - archive_write_data to write the entry data
 *   5) archive_write_close to close the output
 *   6) archive_write_finish to cleanup the writer and release resources
 */
__LA_DECL struct archive	*archive_write_new(void);
__LA_DECL int		 archive_write_set_bytes_per_block(struct archive *,
		     int bytes_per_block);
__LA_DECL int		 archive_write_get_bytes_per_block(struct archive *);
/* XXX This is badly misnamed; suggestions appreciated. XXX */
__LA_DECL int		 archive_write_set_bytes_in_last_block(struct archive *,
		     int bytes_in_last_block);
__LA_DECL int		 archive_write_get_bytes_in_last_block(struct archive *);

/* The dev/ino of a file that won't be archived.  This is used
 * to avoid recursively adding an archive to itself. */
__LA_DECL int		 archive_write_set_skip_file(struct archive *, dev_t, ino_t);

__LA_DECL int		 archive_write_set_compression_bzip2(struct archive *);
__LA_DECL int		 archive_write_set_compression_compress(struct archive *);
__LA_DECL int		 archive_write_set_compression_gzip(struct archive *);
__LA_DECL int		 archive_write_set_compression_none(struct archive *);
__LA_DECL int		 archive_write_set_compression_program(struct archive *,
		     const char *cmd);
/* A convenience function to set the format based on the code or name. */
__LA_DECL int		 archive_write_set_format(struct archive *, int format_code);
__LA_DECL int		 archive_write_set_format_by_name(struct archive *,
		     const char *name);
/* To minimize link pollution, use one or more of the following. */
__LA_DECL int		 archive_write_set_format_ar_bsd(struct archive *);
__LA_DECL int		 archive_write_set_format_ar_svr4(struct archive *);
__LA_DECL int		 archive_write_set_format_cpio(struct archive *);
__LA_DECL int		 archive_write_set_format_cpio_newc(struct archive *);
__LA_DECL int		 archive_write_set_format_mtree(struct archive *);
/* TODO: int archive_write_set_format_old_tar(struct archive *); */
__LA_DECL int		 archive_write_set_format_pax(struct archive *);
__LA_DECL int		 archive_write_set_format_pax_restricted(struct archive *);
__LA_DECL int		 archive_write_set_format_shar(struct archive *);
__LA_DECL int		 archive_write_set_format_shar_dump(struct archive *);
__LA_DECL int		 archive_write_set_format_ustar(struct archive *);
__LA_DECL int		 archive_write_open(struct archive *, void *,
		     archive_open_callback *, archive_write_callback *,
		     archive_close_callback *);
__LA_DECL int		 archive_write_open_fd(struct archive *, int _fd);
__LA_DECL int		 archive_write_open_filename(struct archive *, const char *_file);
/* A deprecated synonym for archive_write_open_filename() */
__LA_DECL int		 archive_write_open_file(struct archive *, const char *_file);
__LA_DECL int		 archive_write_open_FILE(struct archive *, FILE *);
/* _buffSize is the size of the buffer, _used refers to a variable that
 * will be updated after each write into the buffer. */
__LA_DECL int		 archive_write_open_memory(struct archive *,
			void *_buffer, size_t _buffSize, size_t *_used);

/*
 * Note that the library will truncate writes beyond the size provided
 * to archive_write_header or pad if the provided data is short.
 */
__LA_DECL int		 archive_write_header(struct archive *,
		     struct archive_entry *);
#if ARCHIVE_VERSION_NUMBER >= 2000000
__LA_DECL __LA_SSIZE_T		 archive_write_data(struct archive *, const void *, size_t);
#else
/* Temporarily allow library to compile with either 1.x or 2.0 API. */
/* This was erroneously declared to return "int" in libarchive 1.x. */
__LA_DECL int		 archive_write_data(struct archive *, const void *, size_t);
#endif
__LA_DECL __LA_SSIZE_T		 archive_write_data_block(struct archive *, const void *, size_t, off_t);
__LA_DECL int		 archive_write_finish_entry(struct archive *);
__LA_DECL int		 archive_write_close(struct archive *);
#if ARCHIVE_VERSION_NUMBER >= 2000000
__LA_DECL int		 archive_write_finish(struct archive *);
#else
/* Temporarily allow library to compile with either 1.x or 2.0 API. */
/* Return value was incorrect in libarchive 1.x. */
__LA_DECL void		 archive_write_finish(struct archive *);
#endif

/*-
 * To create objects on disk:
 *   1) Ask archive_write_disk_new for a new archive_write_disk object.
 *   2) Set any global properties.  In particular, you should set
 *      the compression and format to use.
 *   3) For each entry:
 *      - construct an appropriate struct archive_entry structure
 *      - archive_write_header to create the file/dir/etc on disk
 *      - archive_write_data to write the entry data
 *   4) archive_write_finish to cleanup the writer and release resources
 *
 * In particular, you can use this in conjunction with archive_read()
 * to pull entries out of an archive and create them on disk.
 */
__LA_DECL struct archive	*archive_write_disk_new(void);
/* This file will not be overwritten. */
__LA_DECL int		 archive_write_disk_set_skip_file(struct archive *,
		     dev_t, ino_t);
/* Set flags to control how the next item gets created. */
__LA_DECL int		 archive_write_disk_set_options(struct archive *,
		     int flags);
/*
 * The lookup functions are given uname/uid (or gname/gid) pairs and
 * return a uid (gid) suitable for this system.  These are used for
 * restoring ownership and for setting ACLs.  The default functions
 * are naive, they just return the uid/gid.  These are small, so reasonable
 * for applications that don't need to preserve ownership; they
 * are probably also appropriate for applications that are doing
 * same-system backup and restore.
 */
/*
 * The "standard" lookup functions use common system calls to lookup
 * the uname/gname, falling back to the uid/gid if the names can't be
 * found.  They cache lookups and are reasonably fast, but can be very
 * large, so they are not used unless you ask for them.  In
 * particular, these match the specifications of POSIX "pax" and old
 * POSIX "tar".
 */
__LA_DECL int	 archive_write_disk_set_standard_lookup(struct archive *);
/*
 * If neither the default (naive) nor the standard (big) functions suit
 * your needs, you can write your own and register them.  Be sure to
 * include a cleanup function if you have allocated private data.
 */
__LA_DECL int	 archive_write_disk_set_group_lookup(struct archive *,
			    void * /* private_data */,
			    __LA_GID_T (*)(void *, const char *, __LA_GID_T),
			    void (* /* cleanup */)(void *));
__LA_DECL int	 archive_write_disk_set_user_lookup(struct archive *,
			    void * /* private_data */,
			    __LA_UID_T (*)(void *, const char *, __LA_UID_T),
			    void (* /* cleanup */)(void *));

/*
 * Accessor functions to read/set various information in
 * the struct archive object:
 */
/* Bytes written after compression or read before decompression. */
__LA_DECL int64_t	 archive_position_compressed(struct archive *);
/* Bytes written to compressor or read from decompressor. */
__LA_DECL int64_t	 archive_position_uncompressed(struct archive *);

__LA_DECL const char	*archive_compression_name(struct archive *);
__LA_DECL int		 archive_compression(struct archive *);
__LA_DECL int		 archive_errno(struct archive *);
__LA_DECL const char	*archive_error_string(struct archive *);
__LA_DECL const char	*archive_format_name(struct archive *);
__LA_DECL int		 archive_format(struct archive *);
__LA_DECL void		 archive_clear_error(struct archive *);
__LA_DECL void		 archive_set_error(struct archive *, int _err,
			    const char *fmt, ...);
__LA_DECL void		 archive_copy_error(struct archive *dest,
			    struct archive *src);

#ifdef __cplusplus
}
#endif

/* This is meaningless outside of this header. */
#undef __LA_DECL

#endif /* !ARCHIVE_H_INCLUDED */
