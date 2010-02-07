/* -*-mode:c++;c-file-style:"bsd";c-basic-offset:4;indent-tabs-mode:nil-*- 
 * Returns rating 0-4 of backtrace file on stdout.
 *  4 - backtrace with complete or almost-complete debuginfos
 *  0 - useless backtrace with no debuginfos
 * Compile:
 *  gcc abrt-rate-backtrace.c -std=c99 -o abrt-rate-backtrace
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
static char* xstrndup(const char *s, int n)
{
  int m;
  char *t;
  
  /* We can just xmalloc(n+1) and strncpy into it, */
  /* but think about xstrndup("abc", 10000) wastage! */
  m = n;
  t = (char*) s;
  while (m) {
    if (!*t) break;
    m--;
    t++;
  }
  n -= m;
  t = (char*) malloc(n + 1);
  t[n] = '\0';
  
  return (char*) memcpy(t, s, n);
}

enum LineRating
{
  // RATING              EXAMPLE
  MissingEverything = 0, // #0 0x0000dead in ?? ()
  MissingFunction   = 1, // #0 0x0000dead in ?? () from /usr/lib/libfoobar.so.4
  MissingLibrary    = 2, // #0 0x0000dead in foobar()
  MissingSourceFile = 3, // #0 0x0000dead in FooBar::FooBar () from /usr/lib/libfoobar.so.4
  Good              = 4, // #0 0x0000dead in FooBar::crash (this=0x0) at /home/user/foobar.cpp:204
  BestRating = Good,
};

static enum LineRating rate_line(const char *line)
{
#define FOUND(x) (strstr(line, x) != NULL)
  /* see the "enum LineRating" comments for possible combinations */
  if (FOUND(" at "))
    return Good;
  const char *function = strstr(line, " in ");
  if (function)
  {
    if (function[4] == '?') /* " in ??" does not count */
    {
      function = NULL;
    }
  }
  bool library = FOUND(" from ");
  if (function && library)
    return MissingSourceFile;
  if (function)
    return MissingLibrary;
  if (library)
    return MissingFunction;
  
  return MissingEverything;
#undef FOUND
}

/* returns number of "stars" to show */
static int rate_backtrace(const char *backtrace)
{
  int i, len;
  int multiplier = 0;
  int rating = 0;
  int best_possible_rating = 0;
  char last_lvl = 0;
  
  /* We look at the frames in reversed order, since:
   * - rate_line() checks starting from the first line of the frame
   * (note: it may need to look at more than one line!)
   * - we increase weight (multiplier) for every frame,
   *   so that topmost frames end up most important
   */
  len = 0;
  for (i = strlen(backtrace) - 1; i >= 0; i--)
  {
    if (backtrace[i] == '#'
	&& (backtrace[i+1] >= '0' && backtrace[i+1] <= '9') /* #N */
	&& (i == 0 || backtrace[i-1] == '\n') /* it's at line start */
      ) {
      /* For one, "#0 xxx" always repeats, skip repeats */
      if (backtrace[i+1] == last_lvl)
	continue;
      last_lvl = backtrace[i+1];
      
      char *s = xstrndup(backtrace + i + 1, len);
      /* Replace tabs with spaces, rate_line() does not expect tabs.
       * Actually, even newlines may be there. Example of multiline frame
       * where " at SRCFILE" is on 2nd line:
       * #3  0x0040b35d in __libc_message (do_abort=<value optimized out>,
       *     fmt=<value optimized out>) at ../sysdeps/unix/sysv/linux/libc_fatal.c:186
       */
      for (char *p = s; *p; p++)
	if (*p == '\t' || *p == '\n')
	  *p = ' ';
      int lrate = rate_line(s);
      multiplier++;
      rating += lrate * multiplier;
      best_possible_rating += BestRating * multiplier;
      //log("lrate:%d rating:%d best_possible_rating:%d s:'%-.40s'", lrate, rating, best_possible_rating, s);
      free(s);
      len = 0; /* starting new line */
    }
    else
    {
      len++;
    }
  }
  
  /* Bogus 'backtrace' with zero frames? */
  if (best_possible_rating == 0)
    return 0;
  
  /* Returning number of "stars" to show */
  if (rating*10 >= best_possible_rating*8) /* >= 0.8 */
    return 4;
  if (rating*10 >= best_possible_rating*6)
    return 3;
  if (rating*10 >= best_possible_rating*4)
    return 2;
  if (rating*10 >= best_possible_rating*2)
    return 1;
  
  return 0;
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s BACKTRACE_FILE\n", argv[0]);
    return 1;
  }

  FILE *fp = fopen(argv[1], "r");
  if (!fp)
  {
    fprintf(stderr, "Cannot open the input file.\n");
    return 2;
  }
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *file = malloc(size + 1);
  if (!file)
  {
    fprintf(stderr, "Malloc error.\n");
    return 3;
  }
  size_t read = fread(file, size, 1, fp);
  if (read != 1)
  {
    fprintf(stderr, "Error while reading file.\n", argv[0]);
    return 4;
  }
  fclose(fp);
  file[size] = '\0';

  int rating = 4;
  /* Do not rate Python backtraces. */
  if (NULL == strstr(file, "Local variables in innermost frame:\n"))
      rating = rate_backtrace(file);

  free(file);
  fprintf(stdout, "%d", rating);
  return 0;
}
