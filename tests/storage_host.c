#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "logic.h"

#define RECORD_STORAGE 1
#define FSAM_READ 1
#define FSOM_OPEN_EXISTING 1
typedef struct { int unused; } Storage;
typedef struct { FILE* stream; DIR* directory; char path[1024]; bool fail_read; } File;
typedef struct { bool directory; } FileInfo;
typedef struct { HashDatabase db; char scanned_dir[FULL_PATH_LEN]; } SubDupFinderApp;
static Storage storage;
static char test_root[1024];
static bool deny_delete;
static const char* unreadable;
static unsigned removals, records;

static bool translate(const char* path,char* out,size_t cap) {
    if(strncmp(path,"/ext/",5) || strstr(path,"..")) return false;
    int written=snprintf(out,cap,"%s/%s",test_root,path+5);
    return written>=0 && (size_t)written<cap;
}
static Storage* furi_record_open(int id) { assert(id==RECORD_STORAGE);records++;return &storage; }
static void furi_record_close(int id) { assert(id==RECORD_STORAGE && records);records--; }
static File* storage_file_alloc(Storage* value) { assert(value==&storage);return calloc(1,sizeof(File)); }
static void storage_file_free(File* file) { assert(!file->stream && !file->directory);free(file); }
static bool storage_file_open(File* file,const char* path,int access,int mode) {
    assert(access==FSAM_READ && mode==FSOM_OPEN_EXISTING);
    if(!translate(path,file->path,sizeof(file->path))) return false;
    file->stream=fopen(file->path,"rb");file->fail_read=unreadable && strstr(path,unreadable);
    return file->stream!=NULL;
}
static uint64_t storage_file_size(File* file) {
    struct stat info;assert(fstat(fileno(file->stream),&info)==0);return (uint64_t)info.st_size;
}
static size_t storage_file_read(File* file,void* data,size_t size) {
    return file->fail_read?0:fread(data,1,size,file->stream);
}
static void storage_file_close(File* file) { assert(fclose(file->stream)==0);file->stream=NULL; }
static bool storage_dir_open(File* file,const char* path) {
    if(!translate(path,file->path,sizeof(file->path)))return false;
    file->directory=opendir(file->path);return file->directory!=NULL;
}
static bool storage_dir_read(File* file,FileInfo* info,char* name,size_t cap) {
    struct dirent* entry=readdir(file->directory);if(!entry)return false;
    snprintf(name,cap,"%s",entry->d_name);char path[2048];
    snprintf(path,sizeof(path),"%s/%s",file->path,entry->d_name);struct stat status;
    assert(stat(path,&status)==0);info->directory=S_ISDIR(status.st_mode);return true;
}
static void storage_dir_close(File* file) { closedir(file->directory);file->directory=NULL; }
static bool file_info_is_dir(const FileInfo* info) { return info->directory; }
static bool storage_simply_remove(Storage* value,const char* path) {
    assert(value==&storage);char host[1024];if(deny_delete || !translate(path,host,sizeof(host)))return false;
    bool success=unlink(host)==0;if(success)removals++;return success;
}

/* PRODUCTION */

static const char* prefix="Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: 433920000\nPreset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\nRAW_Data: ";
static void write_copy(const char* name,const char* suffix) {
    char path[2048];snprintf(path,sizeof(path),"%s/dup-test/%s",test_root,name);
    FILE* stream=fopen(path,"wb");assert(stream);
    assert(fwrite(prefix,1,strlen(prefix),stream)==strlen(prefix));
    assert(fwrite(suffix,1,strlen(suffix),stream)==strlen(suffix));
    assert(fputc('\n',stream)!=EOF);assert(fclose(stream)==0);
}
static bool exists(const char* name) {
    char path[2048];snprintf(path,sizeof(path),"%s/dup-test/%s",test_root,name);return access(path,F_OK)==0;
}
int main(int argc,char** argv) {
    assert(argc==3);snprintf(test_root,sizeof(test_root),"%s",argv[1]);
    char folder[2048];snprintf(folder,sizeof(folder),"%s/dup-test",test_root);assert(mkdir(folder,0700)==0);
    SubDupFinderApp* app=calloc(1,sizeof(*app));assert(app);ScanStats stats;
    write_copy("a.sub","68E077A23D03B9D8");
    write_copy("b.sub",!strcmp(argv[2],"collision")?"616570B36095A746":"68E077A23D03B9D8");
    if(!strcmp(argv[2],"short-read"))unreadable="b.sub";
    assert(storage_scan_directory(app,"/ext/dup-test",&stats));
    if(!strcmp(argv[2],"collision") || !strcmp(argv[2],"short-read")) {
        assert(app->db.num_groups==0);assert(removals==0 && exists("a.sub") && exists("b.sub"));
    } else {
        assert(app->db.num_groups==1 && app->db.groups[0].count==2);
        if(!strcmp(argv[2],"selected-changed"))write_copy("a.sub","different-payload");
        if(!strcmp(argv[2],"peer-changed"))write_copy("b.sub","different-payload");
        if(!strcmp(argv[2],"peer-missing")) { char path[2048];snprintf(path,sizeof(path),"%s/b.sub",folder);assert(unlink(path)==0); }
        if(!strcmp(argv[2],"delete-failure"))deny_delete=true;
        bool deleted=delete_checked(app,"a.sub");
        if(!strcmp(argv[2],"delete"))assert(deleted && removals==1 && !exists("a.sub") && exists("b.sub"));
        else assert(!deleted && removals==0 && exists("a.sub") && app->db.count==2);
    }
    assert(records==0);free(app);return 0;
}
