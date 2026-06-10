#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <openssl/evp.h>
#include "add.h"
#include <sys/stat.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <stdint.h>

typedef struct
{
    char **file_paths;
    int capacity;
    int size;
} string_dynamic_array;

typedef struct
{
    unsigned char hash[32];
    char path[PATH_MAX];
    uint32_t file_size;
    uint64_t time;
} index_el;

unsigned char *hash_file(const char *name, unsigned char *file_buffer, size_t buffer_size)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, file_buffer, buffer_size);
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);

    EVP_MD_CTX_free(mdctx);

    unsigned char *file_hash = malloc(hash_len);
    memcpy(file_hash, hash, hash_len);

    return file_hash;
}

unsigned char *create_file_buffer(const char *file_path, size_t *buffer_size)
{
    struct stat st;

    if (stat(file_path, &st) != 0)
    {
        perror("Error creating file_buffer");
        return NULL;
    }
    size_t header_size = 5;

    unsigned char *file_buffer = malloc(st.st_size + header_size);

    memcpy(file_buffer, "blob", 5);

    FILE *file = fopen(file_path, "rb");
    fread(file_buffer + header_size, 1, st.st_size, file);

    *buffer_size = header_size + st.st_size;

    fclose(file);
    return file_buffer;
}

unsigned char *create_blob(const char *file_name)
{
    size_t buffer_size = 0;
    unsigned char *buffer = create_file_buffer(file_name, &buffer_size);

    unsigned char *hex_file_name = hash_file(file_name, buffer, buffer_size);

    char folder_path[16];
    snprintf(folder_path, sizeof(folder_path), ".nit/obj/%02x", hex_file_name[0]);

    char file_path[128];
    int fp = snprintf(file_path, sizeof(file_path), "%s/", folder_path);

    for (int i = 1; i < 32; ++i)
    {
        fp += snprintf(file_path + fp, sizeof(file_path) - fp, "%02x", hex_file_name[i]);
    }

    if (mkdir(folder_path, 0777) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "Error: Failed to create %s BLOB file.", file_name);
        return NULL;
    }

    FILE *file = fopen(file_path, "wb");
    fwrite(buffer, 1, buffer_size, file);
    fclose(file);

    free(buffer);

    return hex_file_name;
}

bool is_new(const char *file_name)
{
    FILE *file = fopen(".nit/index", "rb");

    index_el el;

    while (fread(&el, sizeof(index_el), 1, file) == 1)
    {
        if (strcmp(el.path, file_name) == 0)
        {
            fclose(file);
            return false;
        }
    }
    fclose(file);
    return true;
}

void update_file_index(const char *file_path, unsigned char *file_hash)
{
    FILE *file = fopen(".nit/index", "r+b");
    struct stat st;

    if (stat(file_path, &st) == 0)
    {
        long pos = 0;
        pos = ftell(file);

        index_el el;

        while (fread(&el, sizeof(index_el), 1, file) == 1)
        {
            if (strcmp(el.path, file_path) == 0)
            {
                memcpy(el.hash, file_hash, 32);
                el.file_size = (uint32_t)st.st_size;
                el.time = (uint64_t)st.st_mtime;

                fseek(file, pos, SEEK_SET);
                fwrite(&el, sizeof(index_el), 1, file);

                break;
            }
            pos = ftell(file);
        }
    }
    fclose(file);
}

void add_file_index(const char *file_path, unsigned char *file_hash, index_el *el)
{

    struct stat st;

    if (stat(file_path, &st) == 0)
    {
        el->file_size = (uint32_t)st.st_size;
        el->time = (uint64_t)st.st_mtime;
        strncpy(el->path, file_path, PATH_MAX);
        memcpy(el->hash, file_hash, 32);
    }
}

void create_file_index(const char *file_path, unsigned char *file_hash)
{
    FILE *file = fopen(".nit/index", "ab");
    index_el cur_el;

    if (!is_new(file_path))
    {
        update_file_index(file_path, file_hash);
        fclose(file);
        return;
    }

    add_file_index(file_path, file_hash, &cur_el);
    fwrite(&cur_el, sizeof(index_el), 1, file);
    fclose(file);
}

void add_file(const char *file_path, string_dynamic_array *file_paths)
{
    unsigned char *file_hash = create_blob(file_path);

    create_file_index(file_path, file_hash);

    if (file_paths->size + 1 == file_paths->capacity)
    {
        file_paths->capacity *= 2;
        file_paths->file_paths = realloc(file_paths->file_paths, file_paths->capacity * sizeof(char *));
    }

    file_paths->file_paths[file_paths->size] = strdup(file_path);
    ++file_paths->size;

    free(file_hash);
}

void walk_dir(const char *dir_path, string_dynamic_array *file_paths)
{
    char path[PATH_MAX];

    struct dirent *entry;
    struct stat st;

    DIR *dir = opendir(dir_path);

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".nit") == 0)
            continue;
        if (strcmp(entry->d_name, ".") == 0)
            continue;
        if (strcmp(entry->d_name, "..") == 0)
            continue;

        if (strcmp(dir_path, ".") == 0)
        {
            snprintf(path, sizeof(path), "%s", entry->d_name);
        }
        else
        {
            snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        }

        if (stat(path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                walk_dir(path, file_paths);
            }
            else if (S_ISREG(st.st_mode))
            {
                add_file(path, file_paths);
            }
        }
    };

    closedir(dir);
    return;
}

int comp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

void nit_add(int argc, char *argv[])
{
    struct stat check_st;
    if (stat(".nit", &check_st) != 0)
    {
        fprintf(stderr, ".nit wasn't initialized!\n");
        return;
    }

    if (argc < 3)
    {
        fprintf(stderr, "Nothin specified, nothing changed.\nMaybe you mean 'nit add .'?\n");
        return;
    }

    string_dynamic_array file_paths;

    file_paths.capacity = 100;
    file_paths.file_paths = malloc(file_paths.capacity * sizeof(char *));
    file_paths.size = 0;

    if (strcmp(argv[2], ".") == 0)
    {
        walk_dir(".", &file_paths);

        qsort(file_paths.file_paths, file_paths.size, sizeof(char *), comp);
    }
    else
    {
        for (int i = 2; i < argc; ++i)
        {
            struct stat file_st;

            if (stat(argv[i], &file_st) == 0)
            {
                if (S_ISDIR(file_st.st_mode))
                {
                    walk_dir(argv[i], &file_paths);
                }
                else
                {
                    add_file(argv[i], &file_paths);
                }
            }
            else
            {
                fprintf(stderr, "Error: %s does not exist!\n", argv[i]);
            }
        }
    }

    for (int i = 0; i <= file_paths.size; ++i)
    {
        free(file_paths.file_paths[i]);
        file_paths.file_paths[i] = NULL;
    }

    free(file_paths.file_paths);
    file_paths.file_paths = NULL;
}