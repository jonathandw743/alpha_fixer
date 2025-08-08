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

typedef struct {
  void *arr;
  size_t offset;
  size_t length;
  size_t capacity;
} Queue;

Queue queue_init() {
  size_t capacity = sizeof(int) * 2 * 1024;
  return (Queue){
      .arr = malloc(capacity), .offset = 0, .length = 0, .capacity = capacity};
}

char queue_nonempty(Queue *queue) { return queue->offset < queue->length; }

void queue_pop(Queue *queue, int *x, int *y) {
  *x = *(int *)(queue->arr + queue->offset);
  *y = *(int *)(queue->arr + queue->offset + sizeof(int));
  queue->offset += sizeof(int) * 2;
}

void queue_push(Queue *queue, int x, int y) {
  if (queue->length + sizeof(int) * 2 > queue->capacity) {
    size_t capacity = queue->capacity * 2;
    queue->arr = realloc(queue->arr, capacity);
    queue->capacity = capacity;
  }
  *(int *)(queue->arr + queue->length) = x;
  *(int *)(queue->arr + queue->length + sizeof(int)) = y;
  queue->length += sizeof(int) * 2;
}

typedef enum { R, G, B, A } Channel;
typedef unsigned char *Col;

Col get_col(unsigned char *data, int width, int x, int y) {
  return (Col)&data[(y * width + x) * 4];
};

void col_set_placeholder(Col col) {
  col[R] = 0;
  col[G] = 0;
  col[B] = 0;
  col[A] = 255;
}

void copy_col(Col to, Col from) {
  to[R] = from[R];
  to[G] = from[G];
  to[B] = from[B];
  to[A] = from[A];
}

void process_pixel(int x, int y, int width, int height, unsigned char *data,
                   unsigned char *visited, Queue *queue, float new_col[],
                   float *num_cols) {
  if (x < 0 || x >= width || y < 0 || y >= height) {
    return;
  }
  unsigned char temp_col[4];
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

int process_file(const char *path) {
  printf("processing file %s\n", path);
  printf("%s\n", path);
  return EXIT_SUCCESS;
}

int process_dir(const char *base_path) {
  printf("processing dir %s\n", base_path);
  DIR *dir = opendir(base_path);
  if (dir == NULL) {
    printf("could not opendir %s\n, skipping", base_path);
    return EXIT_FAILURE;
  }
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0) {
      printf("skipping . (expected)\n");
      continue;
    }
    if (strcmp(entry->d_name, "..") == 0) {
      printf("skipping .. (expected)\n");
      continue;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
    // printf("%s\n", path);
    struct stat statbuf;
    if (stat(path, &statbuf) != EXIT_SUCCESS) {
      printf("could not get file information for %s, skipping\n", path);
      continue;
    }
    if (S_ISDIR(statbuf.st_mode)) {
      if (process_dir(path) != EXIT_SUCCESS) {
        printf("error processing dir %s\n", path);
      }
    } else if (S_ISREG(statbuf.st_mode)) {
      if (process_file(path) != EXIT_SUCCESS) {
        printf("error processing file %s\n", path);
      }
    } else {
      printf("could not process %s, unexpected file type, skipping\n", path);
    }
  }
  if (closedir(dir) != EXIT_SUCCESS) {
    printf("error in closedir %s\n", base_path);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  int alpha_threshold = 0;
  int recolorize_transparent_pixels = 0;
  int make_transparent_opaque = 0;
  char *in_dir = NULL;
  char *out_dir = NULL;
  if (argc == 2 && (strcmp(argv[1], "-h") || strcmp(argv[1], "--help"))) {
    printf("--alpha-threshold or -t <number> to set alpha threshold below "
           "which, a pixel is considered transparent\n"
           "--recolorize-transparent or -r to fix the RGB values of "
           "transparent pixels\n"
           "--make-transparent-opaque or -o to make transparent pixels "
           "opaque at the end of the program (for testing)\n"
           "first non-qualified argument will be the input path\n"
           "second non-qualified argument will be the output path\n");
  }
  for (size_t argi = 1; argi < argc; argi++) {
    if (strcmp(argv[argi], "--alpha-threshold") == 0 ||
        strcmp(argv[argi], "-t") == 0) {
      argi++;
      alpha_threshold = atoi(argv[argi]);
    } else if (strcmp(argv[argi], "--recolorize-transparent") == 0 ||
               strcmp(argv[argi], "-r") == 0) {
      recolorize_transparent_pixels = 1;
    } else if (strcmp(argv[argi], "--make-transparent-opaque") == 0 ||
               strcmp(argv[argi], "-o") == 0) {
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

  // maybe the most insecure code ever
  char command[1024] = "find ";
  strcat(command, in_dir);
  FILE *fp = popen(command, "r");
  if (fp == NULL) {
    perror("popen failed");
    return EXIT_FAILURE;
  }

  char path[1024];
  while (fgets(path, sizeof(path), fp) != NULL) {
    int path_length;
    for (path_length = 0; path_length < sizeof(path) && path[path_length];
         path_length++)
      ;
    path[path_length - 1] = '\0';
    path_length--;
    printf("processing %s\n", path);
    char new_path[1024];
    strcpy(new_path, out_dir);
    strcat(new_path, path + strlen(in_dir));
    if (path_length < 5 || strcmp(path + path_length - 4, ".png") != 0) {
      printf("creating dir %s\n", new_path);
      mkdir(new_path, 0777);
      continue;
    }
    printf("creating image %s\n", new_path);
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 0);
    if (!data) {
      printf("Failed to load image, skipping\n");
      continue;
    }
    if (channels != 4) {
      printf("Image does not have 4 channels, skipping");
      continue;
    }
    if (recolorize_transparent_pixels) {
      unsigned char *visited = malloc(width * height);
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
      size_t offset = 0;
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
        float new_col[3] = {0, 0, 0};
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
    stbi_write_png(new_path, width, height, 4, data, width * 4);
    stbi_image_free(data);
  }

  if (pclose(fp) == -1) {
    perror("pclose failed");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
