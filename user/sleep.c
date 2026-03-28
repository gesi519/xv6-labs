#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  //  Check if the number of arguments is correct
  if(argc != 2){
    fprintf(2, "Usage: sleep ticks\n");
    exit(1);
  }
  //  check if the argument is a valid number
  int n = atoi(argv[1]);
  if(n < 0){
    fprintf(2, "Error: ticks must be a non-negative integer\n");
    exit(1);
  }

  sleep(n);
  exit(0);
}