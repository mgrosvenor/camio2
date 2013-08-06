#include <stdio.h>


int main(int argc, char** argv)
{
    int a;
    printf("Hello  world\n");
    #if NDEBUG
    printf("Release!\n");
    #endif

    return 0;
}

