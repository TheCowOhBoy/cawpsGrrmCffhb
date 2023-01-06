#include <stdio.h>
#include <stdlib.h>

#define TEXT_SIZE 1000

int main() {   
  FILE *file;
  char text[TEXT_SIZE];
  file = fopen("./output.txt", "w");
  printf("Don't be afraid, George, you can do it:\n");
  fgets(text, TEXT_SIZE, stdin);
  fputs(text, file);
  fclose(file);
  return 0;
}

