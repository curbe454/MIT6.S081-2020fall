#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char pingmsg[] = "%d: received ping\n";
    char pongmsg[] = "%d: received pong\n";

    int p[2];
    pipe(p);

    int pid = fork();
    if (pid == 0) {
        char buf[1];

        close(0);
        dup(p[0]);
        if (read(0, buf, 1) > 0) {
            fprintf(2, pingmsg, getpid());
            write(p[1], buf, 1);
            exit(0);
        } else 
            exit(1);
    } else {
        char buf[2] = "pe";

        write(p[1], buf, 1);

        close(0);
        dup(p[0]);
        if (read(0, buf+1, 1) > 0) {
            if (buf[0] == buf[1])
                fprintf(2, pongmsg, getpid());
            //fprintf(2, "%s\n", buf);
            exit(0);
        } else
            exit(1);
    }
}
