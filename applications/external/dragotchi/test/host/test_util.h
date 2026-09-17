#pragma once
#include <stdio.h>
extern int g_fails;
#define CHECK(c) do { if(!(c)){ printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); g_fails++; } } while(0)
