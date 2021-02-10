#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char **argv)
{
  char val;
  int n;
  
  while (1) {
    n=0;
    val = getc(stdin); // read ascii command char
    write(1,&val,1);   // write
    while (1) {
      val = getc(stdin);
      //      printf("val:%hhx\n", val);
      if (val >= '0' && val <= '9') { // numeric byte value in ascii
        ungetc(val, stdin);          
        n=scanf("%hhd", &val);
        assert(n==1);
	write(1,&val,1);
      } else if (val == '\n') { // newline we are done
	write(1,&val,1); // write terminating newline 
	break;           // break out of inner loop
      } 
      // ignore anything else
    }	    
  }
}
