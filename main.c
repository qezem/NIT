#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "init/init.h"
#include "add/add.h"

int main(int argc, char *argv[])
{
    if (strcmp(argv[1], "init") == 0)
    {
        nit_init();
    }
    else if (strcmp(argv[1], "add") == 0)
    {
        nit_add(argc, argv);
    }
}