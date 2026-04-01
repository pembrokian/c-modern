#include <alloca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct mc_string {
    const char* ptr;
    int64_t len;
};

struct mc_allocator {
    uintptr_t raw;
};

struct mc_buffer_u8 {
    uint8_t* ptr;
    int64_t len;
    int64_t cap;
    struct mc_allocator* alloc;
};

struct mc_slice_cstr {
    struct mc_string* ptr;
    int64_t len;
};

extern int32_t __mc_hosted_entry(struct mc_slice_cstr args);

static struct mc_allocator k_default_allocator = {0};

static void mc_zero_buffer_u8(struct mc_buffer_u8* out, struct mc_allocator* alloc) {
    if (out == NULL) {
        return;
    }

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;
    out->alloc = alloc;
}

static char* mc_copy_string_to_cstr(struct mc_string text) {
    if (text.len < 0) {
        return NULL;
    }

    const size_t size = (size_t) text.len;
    char* buffer = (char*) malloc(size + 1);
    if (buffer == NULL) {
        return NULL;
    }

    if (size > 0) {
        memcpy(buffer, text.ptr, size);
    }
    buffer[size] = '\0';
    return buffer;
}

struct mc_allocator* __mc_mem_default_allocator(void) {
    return &k_default_allocator;
}

struct mc_buffer_u8* __mc_mem_buffer_new_u8(struct mc_allocator* alloc, int64_t len) {
    struct mc_allocator* actual_alloc = alloc != NULL ? alloc : &k_default_allocator;

    if (len < 0) {
        return NULL;
    }

    struct mc_buffer_u8* out = (struct mc_buffer_u8*) malloc(sizeof(struct mc_buffer_u8));
    if (out == NULL) {
        return NULL;
    }
    mc_zero_buffer_u8(out, actual_alloc);

    const size_t requested = (size_t) len;
    const size_t allocation_size = requested == 0 ? 1 : requested;
    uint8_t* data = (uint8_t*) malloc(allocation_size);
    if (data == NULL) {
        free(out);
        return NULL;
    }

    out->ptr = data;
    out->len = len;
    out->cap = len;
    out->alloc = actual_alloc;
    return out;
}

void __mc_mem_buffer_free_u8(struct mc_buffer_u8* buf) {
    if (buf == NULL) {
        return;
    }

    free(buf->ptr);
    free(buf);
}

int64_t __mc_mem_buffer_len_u8(struct mc_buffer_u8* buf) {
    if (buf == NULL) {
        return 0;
    }
    return buf->len;
}

int32_t __mc_io_write(struct mc_string text) {
    const size_t size = text.len < 0 ? 0 : (size_t) text.len;
    const size_t written = fwrite(text.ptr, 1, size, stdout);
    if (written != size || fflush(stdout) != 0) {
        return 1;
    }
    return 0;
}

int32_t __mc_io_write_line(struct mc_string text) {
    if (__mc_io_write(text) != 0) {
        return 1;
    }
    if (fputc('\n', stdout) == EOF || fflush(stdout) != 0) {
        return 1;
    }
    return 0;
}

int64_t __mc_fs_file_size(struct mc_string path) {
    int64_t size = -1;
    char* c_path = mc_copy_string_to_cstr(path);
    if (c_path == NULL) {
        return -1;
    }

    struct stat file_stat;
    if (stat(c_path, &file_stat) == 0) {
        size = (int64_t) file_stat.st_size;
    }

    free(c_path);
    return size;
}

struct mc_buffer_u8* __mc_fs_read_all(struct mc_string path, struct mc_allocator* alloc) {
    struct mc_allocator* actual_alloc = alloc != NULL ? alloc : &k_default_allocator;

    char* c_path = mc_copy_string_to_cstr(path);
    if (c_path == NULL) {
        return NULL;
    }

    FILE* file = fopen(c_path, "rb");
    free(c_path);
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long raw_size = ftell(file);
    if (raw_size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    struct mc_buffer_u8* out = __mc_mem_buffer_new_u8(actual_alloc, (int64_t) raw_size);
    if (out == NULL) {
        fclose(file);
        return NULL;
    }

    const size_t target_size = (size_t) raw_size;
    const size_t read_size = target_size == 0 ? 0 : fread(out->ptr, 1, target_size, file);
    fclose(file);

    if (read_size != target_size) {
        __mc_mem_buffer_free_u8(out);
        return NULL;
    }

    out->len = (int64_t) read_size;
    out->cap = (int64_t) read_size;
    out->alloc = actual_alloc;
    return out;
}

int main(int argc, char** argv) {
    struct mc_string* borrowed_args = NULL;
    if (argc > 0) {
        borrowed_args = (struct mc_string*) alloca(sizeof(struct mc_string) * (size_t) argc);
        for (int index = 0; index < argc; ++index) {
            borrowed_args[index].ptr = argv[index];
            borrowed_args[index].len = (int64_t) strlen(argv[index]);
        }
    }

    struct mc_slice_cstr args = {
        .ptr = borrowed_args,
        .len = (int64_t) argc,
    };
    return __mc_hosted_entry(args);
}