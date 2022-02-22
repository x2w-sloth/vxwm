#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "util.h"
#include "vxwm.h"

#ifdef VXWM_DEBUG

void xassert(bool assertion, const char *emsg)
{
  if (assertion == true)
    return;
  die(emsg);
}

#endif // VXWM_DEBUG

void *xmalloc(size_t size)
{
  void *mem = malloc(size);
  if (!mem)
    die("failed to allocate memory");
  return mem;
}

void *xrealloc(void *mem, size_t new_size)
{
  void *new_mem = realloc(mem, new_size);
  if (!new_mem)
    die("failed to reallocate memory");
  return new_mem;
}

void xfree(void *mem)
{
  if (!mem)
    return;
  free(mem);
}

void die(const char *emsg)
{
  fputs(emsg, VXWM_ERR_FILE);
  fputc('\n', VXWM_ERR_FILE);
  exit(EXIT_FAILURE);
}

// vim: ts=2:sw=2:et
