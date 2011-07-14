/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

#include <linux/version.h>

#define LINUX_VERSION_CODE_FOR(major, minor, patch) \
  (((major & 255) << 16) | ((minor & 255) << 8) | (patch & 255))

#define LINUX_VERSION_AT_LEAST(major, minor, patch) \
  (LINUX_VERSION_CODE >= LINUX_VERSION_CODE_FOR(major, minor, patch))

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* fdatasync(2) is available */
#define HAVE_FDATASYNC 1

/* futimes(2) is available */
#define HAVE_FUTIMES 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* pread(2) and pwrite(2) are available */
#define HAVE_PREADWRITE 1

/* readahead(2) is available (linux) */
#define HAVE_READAHEAD 1

/* sendfile(2) is available and supported */
#define HAVE_SENDFILE 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* sync_file_range(2) is available */
#define HAVE_SYNC_FILE_RANGE LINUX_VERSION_AT_LEAST(2, 6, 17)

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "libeio"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.0"
