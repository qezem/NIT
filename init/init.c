#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

int nit_init(void)
{

    if (mkdir(".nit", 0777) != 0)
    {
        if (errno == EEXIST)
        {
            perror(".nit is already initialized\n");
            return 1;
        }
        perror("Failed to initialize .nit");
        return 1;
    }

    if (mkdir(".nit/obj", 0777) != 0)
    {
        perror("Failed to create obj directory\n");
        return 1;
    }

    FILE *file = fopen(".nit/index", "wb");

    if (!file)
    {
        perror("Failed to create index file");
        return 1;
    }

    fclose(file);
    return 0;
}