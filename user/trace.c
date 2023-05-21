#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9'))
  {
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }
  // 这是一个用户态的进程，需要系统调用.          trace是user.h中申明的一个函数.
  // trace在这里是一个封装好的系统调用，它可以来设置需要追踪的系统调用，然后再下面的exec中，程序继承了这个trace追踪的系统调用，然后每一行都要打印出来。
  // 难道不需要设置一个全局变量来表示需要追踪哪些系统调用号吗？
  // 这一点有些犹豫，毕竟我在proc结构体里面没有找到这个变量
  // 没有自己加一个就可以了(其实我的猜想是正确的，可是不知道为什么却不敢加这一行)
  if (trace(atoi(argv[1])) < 0)
  {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  for (i = 2; i < argc && i < MAXARG; i++)
  {
    nargv[i - 2] = argv[i];
  }
  // 要执行的程序
  exec(nargv[0], nargv);
  exit(0);
}
