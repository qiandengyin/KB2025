#ifndef GBK2UTF2UNI_H
#define GBK2UTF2UNI_H

#include "stdlib.h"
#include "stdint.h"

// unsigned short getun(unsigned short gb);
// unsigned short getgb(unsigned short unicode);
// int gb2unicode(char *unicode,char*gb);
// int unicode2gb(char*gb,char * unicode);
// int unicode2utf8(char * utf, char * unicode);
// int utf82unicode(char * unicode, char * utf);
// int gb2utf8( char * utf, char * gb);

int utf82gbk(char *gb, int gb_size, char *utf);

#endif
