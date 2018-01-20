#include <stdlib.h>

int main(int argc, char** argv)
{
    int* p = (int*)malloc(256);
    free(p);
    return 0;
}
