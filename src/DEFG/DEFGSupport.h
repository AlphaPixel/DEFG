// DEFGSupport.h
// support for running DEFG outside of WCS/VNS
// created from scratch and from DEFG.cpp on
// Fri Aug 24, 2001 by CXH


#include <stddef.h>
#include <stdio.h>

class DEFG;

#define APPMEM_CLEAR 						 1

void *malloc(size_t AllocSize, unsigned long int Flags);
void free(void *Me);
double GetSystemTimeFP(void);


int LoadPoints(char *InFile, DEFG *DG);
