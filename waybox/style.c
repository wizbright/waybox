#define _XOPEN_SOURCE 700 // for realpath

#include "waybox/style.h"
#include "waybox/stringop.h"
#include "waybox/util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void to_lower_string(char *str, char *out) {
    for(int i = 0; i<32 &&str[i]; i++){
      out[i] = tolower(str[i]);
      out[i+1] = 0;
    }
}

/* Fowler/Noll/Vo (FNV) hash function, variant 1a */
static size_t fnv1a_hash(const char* cp)
{
    size_t hash = 0x811c9dc5;
    while (*cp) {
        hash ^= (unsigned char) *cp++;
        hash *= 0x01000193;
    }
    return hash;
}

typedef void parse_func(int argc, char **argv, int *target);

typedef struct {
    char* name;
    parse_func *cmd;
} styleProperty;

typedef struct {
    char* name;
    int flag;
} styleFlag;

int getPropertyIndex(styleProperty *props, char *name) {
    for(int i=0;; i++) {
        styleProperty *p = &props[i];
        if (p->name == 0) {
            break;
        }
        if (strcmp(p->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void parseValue(int argc, char **argv, int *target) {

    styleFlag flagMap[] = {
        { "solid", (int)sf_solid },
        { "flat", (int)sf_flat },
        { "raised", (int)sf_raised },
        { "diagonal", (int)sf_diagonal },
        { "crossdiagonal", (int)sf_crossdiagonal },
        { "border", (int)sf_border },
        { "bevel", (int)sf_bevel },
        { "bevel1", (int)sf_bevel1 },
        { "bevel2", (int)sf_bevel2 },
        { "gradient", (int)sf_gradient },
        { "interlaced", (int)sf_interlaced },
        { "sunken", (int)sf_sunken },
        { "vertical", (int)sf_vertical },
        { "horizontal", (int)sf_horizontal },
        { 0, 0 }
    };

    int flags = 0;
    char lowered[255];
    for(int i=0; i<argc; i++) {
        for(int j=0;; j++) {
            if (flagMap[j].name == 0) {
                break;
            }

            to_lower_string(argv[i], lowered);

            if (strcmp(flagMap[j].name, lowered) == 0) {
                flags = flags | flagMap[j].flag;
            }
        }

        // printf("%s %d??\n", argv[i], (int)flagMap[0].flag);
    }

    // is file path?

    // is font?

    *target = flags;
}

void parseInt(int argc, char **argv, int *target) {
    *target = strtol(argv[1], NULL, 10);
}

void parseColor(int argc, char **argv, int *target) {
    uint32_t color;
    if (parse_color(argv[1], &color)) {
        *target = color;
    }
}

void load_style(struct wb_style *config_style, const char *path) {

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Unable to open %s for reading\n", path);
        return;
    }

    struct wb_style style;
    memset(&style, 0, sizeof(struct wb_style));
    int *styleFirstProp = &style.toolbar;

    style.hash = fnv1a_hash(path);

    styleProperty pointerMap[] = {
        #include "style_map.h"
        { 0, 0 }
    };

    char *line = NULL;
    size_t line_size = 0;
    size_t nread;
    while (!feof(f)) {
        nread = getline(&line, &line_size, f);
        if (!nread) {
            continue;
        }
        int argc;
        char **argv = split_args(line, &argc);

        if (argc > 0) {
            int idx = getPropertyIndex(pointerMap, argv[0]);
            if (idx == -1) {
                continue;
            }

            pointerMap[idx].cmd(argc, argv, &styleFirstProp[idx]);
        }

    }

    fclose(f);


    memcpy(config_style, &style, sizeof(struct wb_style));

}
