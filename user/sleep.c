#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if (argc < 2) {
  wrong:
    char prompt[] = "Usage: sleep <seconds>\n";
    write(2, prompt, strlen(prompt));
    exit(1);
  } else {
    int n;

    // parse arg
    n = atoi(argv[1]);
    if(n < 0)
      goto wrong;

    sleep(n);
  }
  exit(0);
}
