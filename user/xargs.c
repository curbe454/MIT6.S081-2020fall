#include "kernel/types.h"

#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXLINE 128

int main(int argc, char *argv[]) {
  char line[MAXLINE];
  char *nargv[MAXARG];
  char *parg;
  int i;
  char IFSs[] = " \n\0";

  // add argv(s) to new argv(s)
  for (i = 0; i < argc; i++)
    nargv[i] = argv[i];

  while (gets(line, MAXLINE)) {
    if (line[0] == '\0')
      break;

    // parse argv in the line
    i = argc;
    char *end = line + strlen(line);
    for (parg = line; parg < end && i < MAXARG;) {
      nargv[i] = parg;
      while (strchr(IFSs, *parg) == 0)
        parg++;
      *parg++ = '\0';
      i++;
    }

    //// for "--verbose"
    // printf("> ");
    // for (int j = 0; j < i; j++)
    //   printf("%s ", nargv[j]);
    // printf("\n");

    if (fork() == 0) {
      exec(argv[1], nargv + 1);
      exit(0);
    } else {
      wait((int *)0);
    }
  }
  exit(0);
}
