#include <alloca.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

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

struct mc_arena {
    uint8_t* ptr;
    int64_t cap;
    int64_t used;
    struct mc_allocator* alloc;
};

struct mc_slice_cstr {
    struct mc_string* ptr;
    int64_t len;
};

struct mc_slice_u8 {
    uint8_t* ptr;
    int64_t len;
};

struct mc_io_poller {
    struct pollfd* entries;
    size_t len;
    size_t cap;
};

struct mc_sync_thread {
    uintptr_t raw;
};

struct mc_sync_mutex {
    uintptr_t raw;
};

struct mc_sync_condvar {
    uintptr_t raw;
};

struct mc_hosted_thread {
    pthread_t thread;
};

struct mc_hosted_mutex {
    pthread_mutex_t mutex;
};

struct mc_hosted_condvar {
    pthread_cond_t cond;
};

struct mc_hosted_thread_start {
    uintptr_t entry;
    uintptr_t ctx;
};

extern int32_t __mc_hosted_entry(struct mc_slice_cstr args);

static struct mc_allocator k_default_allocator = {0};

struct mc_dir_entry {
    char* name;
    size_t len;
    int is_dir;
};

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

static void mc_free_dir_entries(struct mc_dir_entry* entries, size_t count) {
    if (entries == NULL) {
        return;
    }

    for (size_t index = 0; index < count; ++index) {
        free(entries[index].name);
    }
    free(entries);
}

static uintptr_t mc_error_code_from_errno(void) {
    return (uintptr_t) (errno == 0 ? 1 : errno);
}

static void mc_store_error(uintptr_t* out_err, uintptr_t err) {
    if (out_err != NULL) {
        *out_err = err;
    }
}

static short mc_poll_events_from_mask(uint32_t interests) {
    short events = 0;
    if ((interests & 1u) != 0u) {
        events |= POLLIN;
    }
    if ((interests & 2u) != 0u) {
        events |= POLLOUT;
    }
    return events;
}

static struct pollfd* mc_find_pollfd(struct mc_io_poller* poller, int32_t file) {
    if (poller == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < poller->len; ++index) {
        if (poller->entries[index].fd == file) {
            return &poller->entries[index];
        }
    }
    return NULL;
}

static int mc_grow_poller(struct mc_io_poller* poller) {
    const size_t next_cap = poller->cap == 0 ? 8u : poller->cap * 2u;
    struct pollfd* next_entries = (struct pollfd*) realloc(poller->entries, next_cap * sizeof(struct pollfd));
    if (next_entries == NULL) {
        return 0;
    }
    poller->entries = next_entries;
    poller->cap = next_cap;
    return 1;
}

static void mc_fill_ipv4_sockaddr(struct sockaddr_in* addr,
                                  uint8_t a,
                                  uint8_t b,
                                  uint8_t c,
                                  uint8_t d,
                                  uint16_t port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(((uint32_t) a << 24u) |
                                  ((uint32_t) b << 16u) |
                                  ((uint32_t) c << 8u) |
                                  (uint32_t) d);
}

static void mc_configure_stream_socket(int fd) {
    const int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
}

static char* mc_join_path(const char* base, size_t base_len, const char* name, size_t name_len) {
    const int needs_separator = base_len > 0 && base[base_len - 1] != '/';
    const size_t total_len = base_len + (needs_separator ? 1u : 0u) + name_len;
    char* joined = (char*) malloc(total_len + 1);
    if (joined == NULL) {
        return NULL;
    }

    size_t cursor = 0;
    if (base_len > 0) {
        memcpy(joined + cursor, base, base_len);
        cursor += base_len;
    }
    if (needs_separator) {
        joined[cursor] = '/';
        ++cursor;
    }
    if (name_len > 0) {
        memcpy(joined + cursor, name, name_len);
        cursor += name_len;
    }
    joined[cursor] = '\0';
    return joined;
}

static int mc_compare_dir_entries(const void* lhs, const void* rhs) {
    const struct mc_dir_entry* left = (const struct mc_dir_entry*) lhs;
    const struct mc_dir_entry* right = (const struct mc_dir_entry*) rhs;
    const int name_cmp = strcmp(left->name, right->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    return right->is_dir - left->is_dir;
}

static void* mc_thread_entry_shim(void* opaque) {
    struct mc_hosted_thread_start* start = (struct mc_hosted_thread_start*) opaque;
    void (*entry)(void*) = NULL;
    void* ctx = NULL;
    if (start != NULL) {
        entry = (void (*)(void*)) (uintptr_t) start->entry;
        ctx = (void*) start->ctx;
        free(start);
    }
    if (entry != NULL) {
        entry(ctx);
    }
    return NULL;
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

struct mc_arena* __mc_mem_arena_init(struct mc_allocator* alloc, int64_t cap) {
    struct mc_allocator* actual_alloc = alloc != NULL ? alloc : &k_default_allocator;
    if (cap < 0) {
        return NULL;
    }

    struct mc_arena* arena = (struct mc_arena*) malloc(sizeof(struct mc_arena));
    if (arena == NULL) {
        return NULL;
    }

    const size_t requested = (size_t) cap;
    const size_t allocation_size = requested == 0 ? 1 : requested;
    uint8_t* data = (uint8_t*) malloc(allocation_size);
    if (data == NULL) {
        free(arena);
        return NULL;
    }

    arena->ptr = data;
    arena->cap = cap;
    arena->used = 0;
    arena->alloc = actual_alloc;
    return arena;
}

void __mc_mem_arena_deinit(struct mc_arena* arena) {
    if (arena == NULL) {
        return;
    }

    free(arena->ptr);
    free(arena);
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

int64_t __mc_io_read(int32_t file, struct mc_slice_u8 bytes, uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    if (bytes.len < 0) {
        mc_store_error(out_err, 1);
        return 0;
    }
    if (bytes.len == 0) {
        return 0;
    }

    errno = 0;
    const ssize_t read_size = read(file, bytes.ptr, (size_t) bytes.len);
    if (read_size < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return 0;
    }
    return (int64_t) read_size;
}

int64_t __mc_io_write_file(int32_t file, struct mc_slice_u8 bytes, uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    if (bytes.len < 0) {
        mc_store_error(out_err, 1);
        return 0;
    }
    if (bytes.len == 0) {
        return 0;
    }

    errno = 0;
#ifdef MSG_NOSIGNAL
    const ssize_t write_size = send(file, bytes.ptr, (size_t) bytes.len, MSG_NOSIGNAL);
#else
    const ssize_t write_size = write(file, bytes.ptr, (size_t) bytes.len);
#endif
    if (write_size < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return 0;
    }
    return (int64_t) write_size;
}

uintptr_t __mc_io_close(int32_t file) {
    errno = 0;
    if (close(file) != 0) {
        return mc_error_code_from_errno();
    }
    return 0;
}

struct mc_io_poller* __mc_io_poller_new(uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    struct mc_io_poller* poller = (struct mc_io_poller*) calloc(1, sizeof(struct mc_io_poller));
    if (poller == NULL) {
        mc_store_error(out_err, 1);
        return NULL;
    }
    return poller;
}

uintptr_t __mc_io_poller_add(struct mc_io_poller* poller, int32_t file, uint32_t interests) {
    if (poller == NULL) {
        return 1;
    }
    if (mc_find_pollfd(poller, file) != NULL) {
        return (uintptr_t) EEXIST;
    }
    if (poller->len == poller->cap && !mc_grow_poller(poller)) {
        return 1;
    }

    poller->entries[poller->len].fd = file;
    poller->entries[poller->len].events = mc_poll_events_from_mask(interests);
    poller->entries[poller->len].revents = 0;
    ++poller->len;
    return 0;
}

uintptr_t __mc_io_poller_set(struct mc_io_poller* poller, int32_t file, uint32_t interests) {
    struct pollfd* entry = mc_find_pollfd(poller, file);
    if (entry == NULL) {
        return (uintptr_t) ENOENT;
    }

    entry->events = mc_poll_events_from_mask(interests);
    entry->revents = 0;
    return 0;
}

uintptr_t __mc_io_poller_remove(struct mc_io_poller* poller, int32_t file) {
    if (poller == NULL) {
        return 1;
    }

    for (size_t index = 0; index < poller->len; ++index) {
        if (poller->entries[index].fd == file) {
            const size_t tail = poller->len - 1;
            if (index != tail) {
                poller->entries[index] = poller->entries[tail];
            }
            --poller->len;
            return 0;
        }
    }
    return (uintptr_t) ENOENT;
}

int64_t __mc_io_poller_wait(struct mc_io_poller* poller,
                            int32_t* files,
                            uint8_t* readable,
                            uint8_t* writable,
                            uint8_t* failed,
                            int64_t capacity,
                            int32_t timeout_ms,
                            uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    if (poller == NULL || capacity < 0) {
        mc_store_error(out_err, 1);
        return 0;
    }

    const int timeout = timeout_ms < 0 ? -1 : timeout_ms;
    errno = 0;
    const int ready = poll(poller->entries, poller->len, timeout);
    if (ready < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return 0;
    }

    const size_t event_capacity = (size_t) capacity;
    size_t emitted = 0;
    if (event_capacity == 0 || ready == 0) {
        return 0;
    }

    for (size_t index = 0; index < poller->len && emitted < event_capacity; ++index) {
        const short revents = poller->entries[index].revents;
        if (revents == 0) {
            continue;
        }

        files[emitted] = poller->entries[index].fd;
        readable[emitted] = (uint8_t) ((revents & (POLLIN | POLLHUP)) != 0 ? 1 : 0);
        writable[emitted] = (uint8_t) ((revents & POLLOUT) != 0 ? 1 : 0);
        failed[emitted] = (uint8_t) ((revents & (POLLERR | POLLNVAL)) != 0 ? 1 : 0);
        ++emitted;
    }
    return (int64_t) emitted;
}

void __mc_io_poller_close(struct mc_io_poller* poller) {
    if (poller == NULL) {
        return;
    }
    free(poller->entries);
    free(poller);
}

int32_t __mc_strings_eq(struct mc_string left, struct mc_string right) {
    if (left.len != right.len) {
        return 0;
    }
    if (left.len <= 0) {
        return 1;
    }
    return memcmp(left.ptr, right.ptr, (size_t) left.len) == 0 ? 1 : 0;
}

int64_t __mc_strings_find_byte(struct mc_string text, uint8_t needle) {
    if (text.len <= 0) {
        return 0;
    }
    for (int64_t index = 0; index < text.len; ++index) {
        if ((uint8_t) text.ptr[index] == needle) {
            return index;
        }
    }
    return text.len;
}

int64_t __mc_strings_trim_space_start(struct mc_string text) {
    int64_t index = 0;
    while (index < text.len) {
        const unsigned char ch = (unsigned char) text.ptr[index];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        ++index;
    }
    return index;
}

int64_t __mc_strings_trim_space_end(struct mc_string text) {
    int64_t index = text.len;
    while (index > 0) {
        const unsigned char ch = (unsigned char) text.ptr[index - 1];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        --index;
    }
    return index;
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

int32_t __mc_fs_is_dir(struct mc_string path) {
    int32_t is_dir = 0;
    char* c_path = mc_copy_string_to_cstr(path);
    if (c_path == NULL) {
        return 0;
    }

    struct stat file_stat;
    if (stat(c_path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
        is_dir = 1;
    }

    free(c_path);
    return is_dir;
}

struct mc_buffer_u8* __mc_fs_list_dir(struct mc_string path, struct mc_allocator* alloc) {
    struct mc_allocator* actual_alloc = alloc != NULL ? alloc : &k_default_allocator;

    char* c_path = mc_copy_string_to_cstr(path);
    if (c_path == NULL) {
        return NULL;
    }

    DIR* dir = opendir(c_path);
    if (dir == NULL) {
        free(c_path);
        return NULL;
    }

    struct mc_dir_entry* entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int failed = 0;
    const size_t base_len = strlen(c_path);

    for (;;) {
        errno = 0;
        struct dirent* dir_entry = readdir(dir);
        if (dir_entry == NULL) {
            if (errno != 0) {
                failed = 1;
            }
            break;
        }

        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        if (count == capacity) {
            const size_t next_capacity = capacity == 0 ? 8u : capacity * 2u;
            struct mc_dir_entry* next_entries =
                (struct mc_dir_entry*) realloc(entries, next_capacity * sizeof(struct mc_dir_entry));
            if (next_entries == NULL) {
                failed = 1;
                break;
            }
            entries = next_entries;
            capacity = next_capacity;
        }

        const size_t name_len = strlen(dir_entry->d_name);
        char* name_copy = (char*) malloc(name_len + 1);
        if (name_copy == NULL) {
            failed = 1;
            break;
        }
        memcpy(name_copy, dir_entry->d_name, name_len + 1);

        char* joined_path = mc_join_path(c_path, base_len, dir_entry->d_name, name_len);
        if (joined_path == NULL) {
            free(name_copy);
            failed = 1;
            break;
        }

        struct stat entry_stat;
        const int stat_ok = lstat(joined_path, &entry_stat) == 0;
        free(joined_path);
        if (!stat_ok) {
            free(name_copy);
            failed = 1;
            break;
        }

        entries[count].name = name_copy;
        entries[count].len = name_len;
        entries[count].is_dir = S_ISDIR(entry_stat.st_mode) ? 1 : 0;
        ++count;
    }

    closedir(dir);
    free(c_path);

    if (failed) {
        mc_free_dir_entries(entries, count);
        return NULL;
    }

    if (count > 1) {
        qsort(entries, count, sizeof(struct mc_dir_entry), mc_compare_dir_entries);
    }

    size_t total_len = 0;
    for (size_t index = 0; index < count; ++index) {
        total_len += entries[index].len + 1u;
        if (entries[index].is_dir) {
            total_len += 1u;
        }
    }

    struct mc_buffer_u8* out = __mc_mem_buffer_new_u8(actual_alloc, (int64_t) total_len);
    if (out == NULL) {
        mc_free_dir_entries(entries, count);
        return NULL;
    }

    size_t cursor = 0;
    for (size_t index = 0; index < count; ++index) {
        if (entries[index].len > 0) {
            memcpy(out->ptr + cursor, entries[index].name, entries[index].len);
            cursor += entries[index].len;
        }
        if (entries[index].is_dir) {
            out->ptr[cursor] = '/';
            ++cursor;
        }
        out->ptr[cursor] = '\n';
        ++cursor;
    }

    out->len = (int64_t) cursor;
    out->cap = (int64_t) cursor;
    out->alloc = actual_alloc;
    mc_free_dir_entries(entries, count);
    return out;
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

int32_t __mc_net_tcp_listen(uint8_t a,
                            uint8_t b,
                            uint8_t c,
                            uint8_t d,
                            uint16_t port,
                            uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    errno = 0;
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return -1;
    }
    mc_configure_stream_socket(fd);

    struct sockaddr_in addr;
    mc_fill_ipv4_sockaddr(&addr, a, b, c, d, port);
    if (bind(fd, (const struct sockaddr*) &addr, sizeof(addr)) != 0) {
        const uintptr_t err = mc_error_code_from_errno();
        close(fd);
        mc_store_error(out_err, err);
        return -1;
    }

    if (listen(fd, 16) != 0) {
        const uintptr_t err = mc_error_code_from_errno();
        close(fd);
        mc_store_error(out_err, err);
        return -1;
    }

    return fd;
}

int32_t __mc_net_accept(int32_t listener, uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    errno = 0;
    const int fd = accept(listener, NULL, NULL);
    if (fd < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return -1;
    }
    mc_configure_stream_socket(fd);
    return fd;
}

int32_t __mc_net_connect_tcp(uint8_t a,
                             uint8_t b,
                             uint8_t c,
                             uint8_t d,
                             uint16_t port,
                             uintptr_t* out_err) {
    mc_store_error(out_err, 0);
    errno = 0;
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        mc_store_error(out_err, mc_error_code_from_errno());
        return -1;
    }
    mc_configure_stream_socket(fd);

    struct sockaddr_in addr;
    mc_fill_ipv4_sockaddr(&addr, a, b, c, d, port);
    if (connect(fd, (const struct sockaddr*) &addr, sizeof(addr)) != 0) {
        const uintptr_t err = mc_error_code_from_errno();
        close(fd);
        mc_store_error(out_err, err);
        return -1;
    }

    return fd;
}

struct mc_sync_thread __mc_sync_thread_spawn(uintptr_t entry,
                                             uintptr_t ctx,
                                             uintptr_t* out_err) {
    struct mc_sync_thread out = {0};
    mc_store_error(out_err, 0);
    if (entry == 0) {
        mc_store_error(out_err, 1);
        return out;
    }

    struct mc_hosted_thread* thread = (struct mc_hosted_thread*) calloc(1, sizeof(struct mc_hosted_thread));
    if (thread == NULL) {
        mc_store_error(out_err, 1);
        return out;
    }

    struct mc_hosted_thread_start* start = (struct mc_hosted_thread_start*) malloc(sizeof(struct mc_hosted_thread_start));
    if (start == NULL) {
        free(thread);
        mc_store_error(out_err, 1);
        return out;
    }
    start->entry = entry;
    start->ctx = ctx;

    const int rc = pthread_create(&thread->thread, NULL, mc_thread_entry_shim, start);
    if (rc != 0) {
        free(start);
        free(thread);
        mc_store_error(out_err, (uintptr_t) rc);
        return out;
    }

    out.raw = (uintptr_t) thread;
    return out;
}

uintptr_t __mc_sync_thread_join(struct mc_sync_thread thread) {
    if (thread.raw == 0) {
        return 1;
    }

    struct mc_hosted_thread* handle = (struct mc_hosted_thread*) thread.raw;
    const int rc = pthread_join(handle->thread, NULL);
    if (rc != 0) {
        return (uintptr_t) rc;
    }

    free(handle);
    return 0;
}

struct mc_sync_mutex __mc_sync_mutex_init(uintptr_t* out_err) {
    struct mc_sync_mutex out = {0};
    mc_store_error(out_err, 0);

    struct mc_hosted_mutex* handle = (struct mc_hosted_mutex*) calloc(1, sizeof(struct mc_hosted_mutex));
    if (handle == NULL) {
        mc_store_error(out_err, 1);
        return out;
    }

    const int rc = pthread_mutex_init(&handle->mutex, NULL);
    if (rc != 0) {
        free(handle);
        mc_store_error(out_err, (uintptr_t) rc);
        return out;
    }

    out.raw = (uintptr_t) handle;
    return out;
}

uintptr_t __mc_sync_mutex_destroy(struct mc_sync_mutex* mu) {
    if (mu == NULL || mu->raw == 0) {
        return 1;
    }

    struct mc_hosted_mutex* handle = (struct mc_hosted_mutex*) mu->raw;
    const int rc = pthread_mutex_destroy(&handle->mutex);
    if (rc != 0) {
        return (uintptr_t) rc;
    }

    free(handle);
    mu->raw = 0;
    return 0;
}

uintptr_t __mc_sync_mutex_lock(struct mc_sync_mutex* mu) {
    if (mu == NULL || mu->raw == 0) {
        return 1;
    }

    struct mc_hosted_mutex* handle = (struct mc_hosted_mutex*) mu->raw;
    const int rc = pthread_mutex_lock(&handle->mutex);
    return rc == 0 ? 0 : (uintptr_t) rc;
}

uintptr_t __mc_sync_mutex_unlock(struct mc_sync_mutex* mu) {
    if (mu == NULL || mu->raw == 0) {
        return 1;
    }

    struct mc_hosted_mutex* handle = (struct mc_hosted_mutex*) mu->raw;
    const int rc = pthread_mutex_unlock(&handle->mutex);
    return rc == 0 ? 0 : (uintptr_t) rc;
}

struct mc_sync_condvar __mc_sync_condvar_init(uintptr_t* out_err) {
    struct mc_sync_condvar out = {0};
    mc_store_error(out_err, 0);

    struct mc_hosted_condvar* handle = (struct mc_hosted_condvar*) calloc(1, sizeof(struct mc_hosted_condvar));
    if (handle == NULL) {
        mc_store_error(out_err, 1);
        return out;
    }

    const int rc = pthread_cond_init(&handle->cond, NULL);
    if (rc != 0) {
        free(handle);
        mc_store_error(out_err, (uintptr_t) rc);
        return out;
    }

    out.raw = (uintptr_t) handle;
    return out;
}

uintptr_t __mc_sync_condvar_destroy(struct mc_sync_condvar* cv) {
    if (cv == NULL || cv->raw == 0) {
        return 1;
    }

    struct mc_hosted_condvar* cond = (struct mc_hosted_condvar*) cv->raw;
    const int rc = pthread_cond_destroy(&cond->cond);
    if (rc != 0) {
        return (uintptr_t) rc;
    }

    free(cond);
    cv->raw = 0;
    return 0;
}

uintptr_t __mc_sync_condvar_wait(struct mc_sync_condvar* cv,
                                 struct mc_sync_mutex* mu) {
    if (cv == NULL || cv->raw == 0 || mu == NULL || mu->raw == 0) {
        return 1;
    }

    struct mc_hosted_condvar* cond = (struct mc_hosted_condvar*) cv->raw;
    struct mc_hosted_mutex* mutex = (struct mc_hosted_mutex*) mu->raw;
    const int rc = pthread_cond_wait(&cond->cond, &mutex->mutex);
    return rc == 0 ? 0 : (uintptr_t) rc;
}

uintptr_t __mc_sync_condvar_signal(struct mc_sync_condvar* cv) {
    if (cv == NULL || cv->raw == 0) {
        return 1;
    }

    struct mc_hosted_condvar* cond = (struct mc_hosted_condvar*) cv->raw;
    const int rc = pthread_cond_signal(&cond->cond);
    return rc == 0 ? 0 : (uintptr_t) rc;
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