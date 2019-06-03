#include <stdio.h>
#include <stdarg.h>
#include "include/praesidio.h"

//#define putchar output_char

#ifndef EOF
# define EOF (-1)
#endif

//#define puts output_string

int puts(const char *string)
{
    int i = 0;
   while(string[i] != '\0')  //standard c idiom for looping through a null-terminated string
    {
        output_char(string[i]) ;  //if we got the EOF value from writing the char
        i++;
    }

    return 0;

}
