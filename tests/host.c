// The firmware's storage, string and heap APIs on the host, for the tests.

#define HOST_C // plain malloc here
#include "host.h"

#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

const char *host_root = "out";
size_t host_heap = 1 << 20;
static size_t heap_used;

// Each block carries its size in front.
typedef union
{
    size_t n;
    max_align_t align;
} Block;

size_t host_heap_free(void)
{
    return host_heap > heap_used ? host_heap - heap_used : 0;
}

void *host_realloc(void *p, size_t n)
{
    Block *b = p ? (Block *)p - 1 : NULL;
    size_t old = b ? b->n : 0;
    if (n > old && n - old > host_heap_free())
    {
        printf("out of memory: %lu more bytes, %lu free\n", (unsigned long)(n - old), (unsigned long)host_heap_free());
        abort();
    }

    b = realloc(b, sizeof(Block) + n);
    heap_used += n - old;
    b->n = n;
    return b + 1;
}

void *host_malloc(size_t n)
{
    return host_realloc(NULL, n);
}

void host_free(void *p)
{
    if (p)
    {
        Block *b = (Block *)p - 1;
        heap_used -= b->n;
        free(b);
    }
}

struct File
{
    FILE *fp;
};

struct FuriString
{
    char *s;
    size_t len, cap;
};

// "/data/x" -> "<host_root>/data/x"; other paths are used as they are.
static const char *host_path(const char *path, char *buf, size_t n)
{
    if (path[0] != '/')
        return path;

    snprintf(buf, n, "%s%s", host_root, path);
    return buf;
}

File *storage_file_alloc(Storage *st)
{
    (void)st;
    return calloc(1, sizeof(File));
}

void storage_file_free(File *f)
{
    free(f);
}

bool storage_file_open(File *f, const char *path, FS_AccessMode access, FS_OpenMode open)
{
    char buf[256];
    const char *mode = open == FSOM_CREATE_ALWAYS ? (access == FSAM_READ_WRITE ? "w+b" : "wb")
                                                  : (access == FSAM_READ ? "rb" : "r+b");
    f->fp = fopen(host_path(path, buf, sizeof(buf)), mode);
    return f->fp != NULL;
}

bool storage_file_close(File *f)
{
    if (f->fp)
        fclose(f->fp);

    f->fp = NULL;
    return true;
}

size_t storage_file_read(File *f, void *buf, size_t n)
{
    return fread(buf, 1, n, f->fp);
}

size_t storage_file_write(File *f, const void *buf, size_t n)
{
    return fwrite(buf, 1, n, f->fp);
}

bool storage_file_seek(File *f, uint32_t offset, bool from_start)
{
    return fseek(f->fp, (long)offset, from_start ? SEEK_SET : SEEK_CUR) == 0;
}

bool storage_file_truncate(File *f)
{
    long pos = ftell(f->fp);
    fflush(f->fp);
#ifdef _WIN32
    return _chsize_s(_fileno(f->fp), pos) == 0;
#else
    return ftruncate(fileno(f->fp), pos) == 0;
#endif
}

bool storage_file_exists(Storage *st, const char *path)
{
    (void)st;
    char buf[256];
    struct stat s;
    return stat(host_path(path, buf, sizeof(buf)), &s) == 0;
}

FS_Error storage_common_rename(Storage *st, const char *from, const char *to)
{
    (void)st;
    char a[256], b[256];
    const char *pb = host_path(to, b, sizeof(b));
    remove(pb); // the firmware overwrites, Windows' rename doesn't
    return rename(host_path(from, a, sizeof(a)), pb) == 0 ? FSE_OK : FSE_INTERNAL;
}

bool storage_simply_remove(Storage *st, const char *path)
{
    (void)st;
    char buf[256];
    return remove(host_path(path, buf, sizeof(buf))) == 0;
}

static void remove_tree(const char *p)
{
    DIR *d = opendir(p);
    if (!d)
    {
        remove(p);
        return;
    }

    struct dirent *e;
    while ((e = readdir(d)))
    {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
        {
            char sub[512];
            snprintf(sub, sizeof(sub), "%s/%s", p, e->d_name);
            remove_tree(sub);
        }
    }

    closedir(d);
    rmdir(p);
}

bool storage_simply_remove_recursive(Storage *st, const char *path)
{
    (void)st;
    char buf[256];
    remove_tree(host_path(path, buf, sizeof(buf)));
    return true;
}

bool storage_simply_mkdir(Storage *st, const char *path)
{
    (void)st;
    char buf[256];
#ifdef _WIN32
    return _mkdir(host_path(path, buf, sizeof(buf))) == 0;
#else
    return mkdir(host_path(path, buf, sizeof(buf)), 0777) == 0;
#endif
}

FuriString *furi_string_alloc(void)
{
    FuriString *s = calloc(1, sizeof(FuriString));
    s->s = calloc(1, 1);
    s->cap = 1;
    return s;
}

FuriString *furi_string_alloc_set_str(const char *v)
{
    FuriString *s = furi_string_alloc();
    furi_string_set(s, v);
    return s;
}

void furi_string_free(FuriString *s)
{
    free(s->s);
    free(s);
}

void furi_string_reserve(FuriString *s, size_t n)
{
    if (n + 1 > s->cap)
    {
        s->s = realloc(s->s, n + 1);
        s->cap = n + 1;
    }
}

void furi_string_reset(FuriString *s)
{
    s->len = 0;
    s->s[0] = '\0';
}

void furi_string_cat_str(FuriString *s, const char *v)
{
    size_t n = strlen(v);
    furi_string_reserve(s, s->len + n);
    memcpy(s->s + s->len, v, n + 1);
    s->len += n;
}

void furi_string_set(FuriString *s, const char *v)
{
    furi_string_reset(s);
    furi_string_cat_str(s, v);
}

void furi_string_push_back(FuriString *s, char c)
{
    char v[2] = {c, '\0'};
    furi_string_cat_str(s, v);
}

void furi_string_printf(FuriString *s, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    furi_string_set(s, buf);
}

const char *furi_string_get_cstr(const FuriString *s)
{
    return s->s;
}

size_t furi_string_size(const FuriString *s)
{
    return s->len;
}

size_t host_strlcpy(char *dst, const char *src, size_t n)
{
    size_t len = strlen(src);
    if (n)
    {
        size_t k = len < n - 1 ? len : n - 1;
        memcpy(dst, src, k);
        dst[k] = '\0';
    }

    return len;
}
