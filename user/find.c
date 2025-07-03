#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NULL 0

char *strstr(char *str, char *target);
char *basename(char *path);
void find(char *path, char *str);

int main(int argc, char *argv[]) {
  if (argc == 1) {
    find(".", NULL);
  } else if (argc == 2) {
    find(".", argv[1]);
  } else if (argc == 3) {
    find(argv[1], argv[2]);
  } else {
    fprintf(2, "Usage: find [PATH [FNAME]]\n");
    exit(1);
  }
  exit(0);
}

// like function strstr in <string.h>
char *strstr(char *s, char *t) {
  int len = strlen(t);
  char *p;
  for (p = s; *p != '\0'; p++) {
    if (*p == t[0])
      if (memcmp(p, t, len) == 0)
        return p;
  }
  return NULL;
}

char *basename(char *path) {
  char *p = path + strlen(path);
  for (; p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}

void find(char *path, char *str) {
  int fd;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: can not open file %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: can not stat file %s\n", path);
    close(fd);
    return;
  }

  if (str == NULL || strstr(path, str) != NULL) {
    // printf("%s %d %d %l\n", path, st.type, st.ino, st.size);
    printf("%s\n", path);
  }
  switch (st.type) {
  case T_DIR: {
    char buf[512], *p;
    struct dirent de;

    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "ls: path \"%s\" too long\n", path);
      break;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      if (strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
        find(buf, str);
      }
    }
    break;
  }
  case T_FILE:
    break;
  }
  close(fd);
}
