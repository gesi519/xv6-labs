#include "../kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  if(argc != 1) {
    fprintf(2, "Usage: pingpong <number of iterations>\n");
    exit(1);
  }
  char buf;

  int pipe1[2], pipe2[2];
  // 创建两条管道
  if(pipe(pipe1) < 0 || pipe(pipe2) < 0) {
    fprintf(2, "Error: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0) {
    fprintf(2, "Error: fork failed\n");
    exit(1);
  }
  // 子进程
  if(pid == 0) {
    close(pipe1[1]); // 关闭管道1的写端
    close(pipe2[0]); // 关闭管道2的读端

    if(read(pipe1[0], &buf, 1) != 1) {
      fprintf(2, "Error: child read failed\n");
      exit(1);
    }
    fprintf(1, "%d: received ping\n", getpid());

    if(write(pipe2[1], &buf, 1) != 1) {
        fprintf(2, "Error: child write failed\n");
        exit(1);
    }

    close(pipe1[0]);
    close(pipe2[1]);

    exit(0);
  }else {
    // 父进程
    close(pipe1[0]); // 关闭管道1的读端
    close(pipe2[1]); // 关闭管道2的写端

    // 向子进程发送一个字节
    buf = 'g';
    if(write(pipe1[1], &buf, 1) != 1) {
        fprintf(2, "Error: parent write failed\n");
        exit(1);
    }

    if(read(pipe2[0], &buf, 1) != 1) {
        fprintf(2, "Error: parent read failed\n");
        exit(1);
    }

    fprintf(1, "%d: received pong\n", getpid());

    close(pipe1[1]);
    close(pipe2[0]);

    wait(0); // 等待子进程结束
    exit(0);
  }

}

