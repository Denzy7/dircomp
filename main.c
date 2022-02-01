#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <string.h>

#include <lz4.h>

#include <dirent.h>


typedef struct filenode
{
    char* name;
    uint32_t sz;
    struct filenode* next;
} filenode;

filenode* head = NULL;
void getfiles(const char* dirstr, uint32_t* count)
{
    DIR* dir = opendir(dirstr);
    if(!dir)
        return;
    struct dirent* entry;
    while((entry = readdir(dir)))
    {
        if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name,".."))
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s",dirstr, entry->d_name);
        FILE* f = fopen(path, "rb+");
        if(f)
        {
            uint32_t cnt = *count;
            cnt++;
            *count = cnt;

            fseek(f, 0, SEEK_END);
            filenode* node = malloc(sizeof(filenode));
            node->name = strdup(path);
            node->sz = ftell(f);

            node->next = head;
            head = node;

            fclose(f);
        }

        getfiles(path, count);

    }

    closedir(dir);


}

int comp(const char* dirstr, const char* out)
{
    uint32_t count = 0;
    getfiles(dirstr, &count);

    if(count)
    {
        printf("compress %u files\n", count);
        /*
         * <count>(4)
         *
         * <decomp-data-sz>(4),<data-sz>(4)<file-str-len>(4),<file-str>(...),<data>(...),
         *
         */

        FILE* f_out = fopen(out, "wb");
        if(!f_out)
            return 0;


        //count
        fwrite(&count, sizeof(uint32_t), 1, f_out);

        filenode* ptr = head;
        filenode* tmp;
        while (ptr) {
            FILE* f_file = fopen(ptr->name, "rb");
            if(f_file)
            {
                printf("%s\n", ptr->name);
                printf("sz : %u\n", ptr->sz);
                void* file_src = malloc(ptr->sz);
                fread(file_src, 1, ptr->sz, f_file);
                fclose(f_file);

                void* file_dst = malloc(ptr->sz);

                //decompsz, filestrln, filestr, datasz, data

                //decomp-data-sz
                fwrite(&ptr->sz, sizeof(uint32_t), 1, f_out);

                //file-str-len
                uint32_t file_strlen = strlen(ptr->name);
                fwrite(&file_strlen, sizeof(uint32_t), 1, f_out);

                //file-str
                fwrite(ptr->name, file_strlen, 1, f_out);

                int32_t compressed = LZ4_compress_default(file_src, file_dst,ptr->sz, ptr->sz);
                if(compressed)
                {

                    float ratio = (float)compressed / (float)ptr->sz;
                    printf("comp : %u, ratio : %.1f%%\n\n", compressed, ratio * 100);

                    //data-sz
                    fwrite(&compressed, sizeof(uint32_t), 1, f_out);

                    //data
                    fwrite(file_dst, 1, compressed, f_out);
                }else
                {
                    printf("comp failed! write raw %u\n\n", ptr->sz);

                    //data-sz
                    fwrite(&ptr->sz, sizeof(uint32_t), 1, f_out);

                    //data
                    fwrite(file_src, ptr->sz, 1, f_out);
                }


                free(file_src);
                free(file_dst);
            }
            ptr = ptr->next;
        }

        fclose(f_out);

    }

    struct filenode* tmp;
    while (head != NULL)
    {
        tmp = head;
        head = head->next;
        free(tmp->name);
        free(tmp);
    }


    return 1;
}

int decomp(const char* file)
{

    FILE* f_in = fopen(file, "rb");
    if(!f_in)
        return 0;

    uint32_t count = 0;
    fread(&count, sizeof(uint32_t), 1, f_in);

    printf("decomp %u files\n", count);

    for(uint32_t i = 0; i < count-1; i++)
    {
        //decompsz, filestrln, filestr, datasz, data
        uint32_t decomp_data_sz = 0;
        fread(&decomp_data_sz, sizeof(uint32_t), 1, f_in);

        uint32_t file_str_sz = 0;
        fread(&file_str_sz, sizeof(uint32_t), 1, f_in);

        char* file_str = malloc(file_str_sz + 1);
        //+1 = \0
        memset(file_str, 0, file_str_sz + 1);
        fread(file_str, file_str_sz, 1, f_in);

        uint32_t data_sz = 0;
        fread(&data_sz, sizeof(uint32_t), 1, f_in);

        void* data_src = malloc(data_sz);
        fread(data_src, data_sz, 1, f_in);

        printf("rd : %s\n", file_str);
        printf("sz : %u\n", data_sz);

        uint32_t decomp = 0;
        void* data_dest = NULL;
        if(decomp_data_sz != data_sz)
        {
            data_dest = malloc(decomp_data_sz);

            decomp = LZ4_decompress_safe(data_src, data_dest, data_sz, decomp_data_sz);

            float ratio = (float)data_sz / (float)decomp ;

            printf("decomp : %u, ratio : %.1f%%\n\n", decomp, ratio * 100);
        }else
        {
            decomp = data_sz;
            data_dest = data_src;

            printf("decomp raw : %u\n", decomp);
        }

        const char* out_file = strrchr(file_str, '/') + 1;
        FILE* f_out = fopen(out_file, "wb");
        if(f_out)
            fwrite(data_dest, decomp, 1, f_out);
        else
            printf("cannot write to %s", out_file);

        free(data_src);
        free(file_str);
        if(decomp_data_sz != data_sz)
        {
            free(data_dest);
        }
    }

    return 1;
}

int main(int argc, char** argv)
{

    const char* help =
            "usage : dircomp [operation] [options]\n\n"
            "operations:\n"
            "-c [directory]: compress a directory\n"
                "\t-o [file] : set an output file to save compressed data\n"
            "-d [file]: decompress a to the current directory\n";
    if(argc < 2)
    {
        printf("%s", help);
        return 1;
    }


    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i],"-c"))
        {
            const char* dirstr = NULL;
            const char* outfile = NULL;

            if(argv[i + 2] && argv[i+3] && !strcmp(argv[i + 2],"-o"))
            {
                outfile = argv[i+3];
                printf("set out file to %s\n", outfile);
            }else
            {
                printf("use -o to set output file\n");
                return 1;
            }

            dirstr = argv[i+1];
            printf("will compress dir %s\n", dirstr);

            comp(dirstr, outfile);
        }else if(!strcmp(argv[i],"-d"))
        {
            if(argv[i + 1])
            {
                printf("will decomp file %s\n", argv[i + 1]);
                decomp(argv[i + 1]);
            }else
            {
                printf("suppy a file to -d to decompress\n");
                return 1;
            }
        }else if(!strcmp(argv[i],"-h"))
        {
            printf("%s", help);
        }
    }

    return 0;
}
