#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BT_BUF_SIZE 100

FILE *fopen(const char *pathname, const char *mode) {

  FILE *(*fopen_real)(const char *pathname, const char *mode) = dlsym(RTLD_NEXT, "fopen");

  if (fopen_real == NULL) { // err
    errno = ELIBACC;
    return NULL;
  }

  FILE *ret = fopen_real(pathname, mode);

  if (ret == NULL) { // err
    return ret;
  }

  bool triggered = false;

  void *stackbuf[BT_BUF_SIZE];
  int btlen = backtrace(stackbuf, BT_BUF_SIZE);
  char **symbols = backtrace_symbols(stackbuf, btlen);

  if (symbols == NULL) { // err
    return ret;
  }

  for (int i = 0; i < btlen; i++) {
    if (strstr(symbols[i], "libOclCpuBackEnd_emu.so")) {
      triggered = true;
    }
  }

  // malloced by backtrace_symbols()
  free(symbols);

  if (triggered) {
    setvbuf(ret, NULL, _IONBF, 0); // ignore any errors
  }

  return ret;
}
