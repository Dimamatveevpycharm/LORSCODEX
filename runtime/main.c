#include <stdio.h>

extern long entry(void);

int main(void) {
  long result = entry();
  printf("entry() = %ld\n", result);
  return 0;
}

