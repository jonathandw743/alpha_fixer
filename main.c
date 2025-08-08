#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int alpha_threshold = 0;
int recolorize_transparent_pixels = 0;
int make_transparent_opaque = 0;
char* in_dir = NULL;
char* out_dir = NULL;

typedef struct {
    int* arr;
    size_t offset;
    size_t length;
    size_t capacity;
} Queue;

Queue queue_init()
{
    size_t capacity = 2 * 1024;
    return (Queue) {
        .arr = malloc(capacity * sizeof(int)), .offset = 0, .length = 0, .capacity = capacity
    };
}

void queue_free(Queue* queue)
{
    free(queue->arr);
}

char queue_nonempty(Queue* queue) { return queue->offset < queue->length; }

void queue_pop(Queue* queue, int* x, int* y)
{
    *x = queue->arr[queue->offset];
    *y = queue->arr[queue->offset + 1];
    queue->offset += 2;
}

void queue_push(Queue* queue, int x, int y)
{
    if (queue->length + 2 > queue->capacity) {
        size_t capacity = queue->capacity * 2;
        queue->arr = realloc(queue->arr, capacity * sizeof(int));
        queue->capacity = capacity;
    }
    queue->arr[queue->length] = x;
    queue->arr[queue->length + 1] = y;
    queue->length += 2;
}

typedef enum { R,
    G,
    B,
    A } Channel;
typedef unsigned char* Col;

Col get_col(unsigned char* data, int width, int x, int y)
{
    return (Col)&data[(y * width + x) * 4];
}

void col_set_placeholder(Col col)
{
    col[R] = 0;
    col[G] = 0;
    col[B] = 0;
    col[A] = 255;
}

void copy_col(Col to, Col from)
{
    to[R] = from[R];
    to[G] = from[G];
    to[B] = from[B];
    to[A] = from[A];
}

void process_pixel(int x, int y, int width, int height, unsigned char* data,
    unsigned char* visited, Queue* queue, float new_col[],
    float* num_cols)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }
    if (visited[y * width + x]) {
        Col col = get_col(data, width, x, y);
        new_col[R] += (float)col[R];
        new_col[G] += (float)col[G];
        new_col[B] += (float)col[B];
        *num_cols += 1;
    } else {
        queue_push(queue, x, y);
    }
}

int process_file(const char* in_path, const char* out_path)
{
    printf("processing file %s -> %s\n", in_path, out_path);
    int width, height, channels;
    unsigned char* data = stbi_load(in_path, &width, &height, &channels, 0);
    if (!data) {
        printf("Failed to load image %s, skipping\n", in_path);
        return EXIT_FAILURE;
    }
    if (channels != 4) {
        printf("Image %s does not have 4 channels, skipping\n", in_path);
        return EXIT_FAILURE;
    }
    if (recolorize_transparent_pixels) {
        unsigned char* visited = malloc(width * height);
        Queue queue = queue_init();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Col col = get_col(data, width, x, y);
                if (col[A] <= alpha_threshold) {
                    visited[y * width + x] = 0;
                } else {
                    visited[y * width + x] = 1;
                    queue_push(&queue, x + 1, y);
                    queue_push(&queue, x - 1, y);
                    queue_push(&queue, x, y + 1);
                    queue_push(&queue, x, y - 1);
                }
            }
        }
        while (queue_nonempty(&queue)) {
            int x, y;
            queue_pop(&queue, &x, &y);
            if (x < 0 || x >= width || y < 0 || y >= height) {
                continue;
            }
            if (visited[y * width + x]) {
                continue;
            }
            visited[y * width + x] = 1;
            Col col = get_col(data, width, x, y);
            float new_col[3] = { 0, 0, 0 };
            float num_cols = 0;
            process_pixel(x + 1, y, width, height, data, visited, &queue, new_col,
                &num_cols);
            process_pixel(x - 1, y, width, height, data, visited, &queue, new_col,
                &num_cols);
            process_pixel(x, y + 1, width, height, data, visited, &queue, new_col,
                &num_cols);
            process_pixel(x, y - 1, width, height, data, visited, &queue, new_col,
                &num_cols);
            new_col[R] /= num_cols;
            new_col[G] /= num_cols;
            new_col[B] /= num_cols;
            col[R] = (unsigned char)new_col[R];
            col[G] = (unsigned char)new_col[G];
            col[B] = (unsigned char)new_col[B];
            col[A] = 0;
        }
        queue_free(&queue);
    }
    if (make_transparent_opaque) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Col col = get_col(data, width, x, y);
                if (col[A] <= alpha_threshold) {
                    col[A] = 255;
                }
            }
        }
    }
    stbi_write_png(out_path, width, height, 4, data, width * 4);
    stbi_image_free(data);
    return EXIT_SUCCESS;
}

int process_path(const char* in_path, const char* out_path);

int process_dir(const char* in_dir, const char* out_dir)
{
    printf("processing dir %s -> %s\n", in_dir, out_dir);
    printf("creating dir %s\n", out_dir);
    mkdir(out_dir, 0777);
    DIR* dir = opendir(in_dir);
    if (dir == NULL) {
        printf("could not opendir %s, skipping\n", in_dir);
        return EXIT_FAILURE;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) {
            printf("skipping . (expected)\n");
            continue;
        }
        if (strcmp(entry->d_name, "..") == 0) {
            printf("skipping .. (expected)\n");
            continue;
        }
        char in_path[1024];
        snprintf(in_path, sizeof(in_path), "%s/%s", in_dir, entry->d_name);
        char out_path[1024];
        snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, entry->d_name);
        printf("in_path: %s, out_path: %s\n", in_path, out_path);
        if (process_path(in_path, out_path) != EXIT_SUCCESS) {
            printf("error in processing path %s -> %s, skipping", in_path, out_path);
        }
    }
    if (closedir(dir) != EXIT_SUCCESS) {
        printf("error in closedir %s\n", in_dir);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int process_path(const char* in_path, const char* out_path)
{
    printf("processing path %s -> %s\n", in_path, out_path);
    struct stat statbuf;
    if (stat(in_path, &statbuf) != EXIT_SUCCESS) {
        printf("could not get file information for %s, skipping\n", in_path);
        return EXIT_FAILURE;
    }
    if (S_ISDIR(statbuf.st_mode)) {
        if (process_dir(in_path, out_path) != EXIT_SUCCESS) {
            printf("error processing dir %s\n", in_path);
        }
    } else if (S_ISREG(statbuf.st_mode)) {
        if (process_file(in_path, out_path) != EXIT_SUCCESS) {
            printf("error processing file %s\n", in_path);
        }
    } else {
        printf("could not process %s, unexpected file type, skipping\n", in_path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    if (argc == 2 && (strcmp(argv[1], "-h") || strcmp(argv[1], "--help"))) {
        printf("--alpha-threshold or -t <number> to set the alpha threshold. Any pixel with alpha less than or equal to this will be considered transparent.\n"
               "--recolorize-transparent or -r to fix the RGB values of transparent pixels.\n"
               "--make-transparent-opaque or -o to make transparent pixels opaque at the end of the program (for testing).\n"
               "The first non-qualified argument will be the input path.\n"
               "The second non-qualified argument will be the output path.\n");
    }
    for (size_t argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--alpha-threshold") == 0 || strcmp(argv[argi], "-t") == 0) {
            argi++;
            alpha_threshold = atoi(argv[argi]);
        } else if (strcmp(argv[argi], "--recolorize-transparent") == 0 || strcmp(argv[argi], "-r") == 0) {
            recolorize_transparent_pixels = 1;
        } else if (strcmp(argv[argi], "--make-transparent-opaque") == 0 || strcmp(argv[argi], "-o") == 0) {
            make_transparent_opaque = 1;
        } else if (in_dir == 0) {
            in_dir = argv[argi];
        } else if (out_dir == 0) {
            out_dir = argv[argi];
        } else {
            perror("Incorrect arguments, use --help or -h");
            return EXIT_FAILURE;
        }
    }
    if (in_dir == NULL || out_dir == NULL) {
        perror("in_dir or out_dir not set");
        return EXIT_FAILURE;
    }
    if (process_path(in_dir, out_dir) != EXIT_SUCCESS) {
        printf("error in processing\n");
    }
    return EXIT_SUCCESS;
}
