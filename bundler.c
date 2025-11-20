#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

/*
 * Simple Bundler in C
 * 
 * Usage: ./bundler <manifest.txt> <bundle.bin>
 * 
 * Manifest format: One relative file path per line (UTF-8, trimmed whitespace).
 * Paths are relative to the current working directory.
 * 
 * Bundle Format (binary, read-only):
 * - Header (9 bytes):
 *   - Magic bytes: 'JSFS' (4 bytes, ASCII).
 *   - Version: 1 (1 byte, unsigned).
 *   - Number of files: uint32 big-endian (4 bytes).
 * - Metadata (variable length, one entry per file):
 *   - Path length: uint16 big-endian (2 bytes, max 65535).
 *   - Path: UTF-8 encoded string (path length bytes, relative path using '/' separator, no leading/trailing '/').
 *   - File size: uint64 big-endian (8 bytes, bytes in content).
 *   - Content offset: uint64 big-endian (8 bytes, absolute byte offset in bundle where content starts).
 * - Contents (concatenated): Raw binary bytes of each file's content, in the order of metadata entries.
 * 
 * Assumptions/Limits:
 * - Up to 2^32 - 1 files.
 * - File paths < 64KB.
 * - File sizes < 2^64 bytes.
 * - No directories in manifest (paths include dirs, e.g., "src/file.txt").
 * - Overwrites bundle if exists.
 * - Errors (missing files/manifest) cause exit(1).
 * - Memory: Loads all contents into RAM (suitable for small bundles).
 * 
 * Compilation: gcc -o bundler bundler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>  // For potential stat, but using fseek/ftell

#define INITIAL_CAPACITY 16

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;

typedef struct {
    char* path;
    size_t plen;
    u64 size;
    u64 offset;
    char* content;
} Entry;

void write_u16_be(FILE* f, u16 v) {
    fputc((v >> 8) & 0xFF, f);
    fputc(v & 0xFF, f);
}

void write_u32_be(FILE* f, u32 v) {
    fputc((v >> 24) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc(v & 0xFF, f);
}

void write_u64_be(FILE* f, u64 v) {
    fputc((v >> 56) & 0xFF, f);
    fputc((v >> 48) & 0xFF, f);
    fputc((v >> 40) & 0xFF, f);
    fputc((v >> 32) & 0xFF, f);
    fputc((v >> 24) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc(v & 0xFF, f);
}

void add_entry(Entry** entries, size_t* capacity, size_t* count, const char* path_str, u64 fsize, char* content) {
    if (*count >= *capacity) {
        *capacity = *capacity ? *capacity * 2 : INITIAL_CAPACITY;
        *entries = realloc(*entries, *capacity * sizeof(Entry));
        if (!*entries) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
    }
    Entry* e = &(*entries)[*count];
    e->path = strdup(path_str);
    if (!e->path) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    e->plen = strlen(path_str);
    e->size = fsize;
    e->offset = 0;  // To be set later
    e->content = content;
    (*count)++;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <manifest.txt> <bundle.bin>\n", argv[0]);
        return 1;
    }

    char* manifest_path = argv[1];
    char* bundle_path = argv[2];

    FILE* mfp = fopen(manifest_path, "r");
    if (!mfp) {
        fprintf(stderr, "Error: Cannot open manifest '%s'\n", manifest_path);
        return 1;
    }

    Entry* entries = NULL;
    size_t capacity = 0;
    size_t count = 0;

    char line[4096];  // Reasonable max path + line
    while (fgets(line, sizeof(line), mfp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') continue;  // Skip empty lines

        FILE* fp = fopen(line, "rb");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", line);
            fclose(mfp);
            // Clean up existing entries
            for (size_t i = 0; i < count; i++) {
                free(entries[i].content);
                free(entries[i].path);
            }
            free(entries);
            return 1;
        }

        fseek(fp, 0, SEEK_END);
        long fsz_long = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsz_long < 0) {
            fprintf(stderr, "Error: Invalid file size for '%s'\n", line);
            fclose(fp);
            fclose(mfp);
            // Cleanup...
            return 1;
        }
        u64 fsize = (u64)fsz_long;

        char* content = malloc(fsize);
        if (!content || fread(content, 1, fsize, fp) != fsize) {
            fprintf(stderr, "Error: Failed to read file '%s'\n", line);
            free(content);
            fclose(fp);
            fclose(mfp);
            // Cleanup...
            return 1;
        }
        fclose(fp);

        add_entry(&entries, &capacity, &count, line, fsize, content);
    }
    fclose(mfp);

    if (count == 0) {
        fprintf(stderr, "Error: No files in manifest\n");
        free(entries);
        return 1;
    }

    // Compute total metadata size
    size_t total_meta = 0;
    for (size_t i = 0; i < count; i++) {
        total_meta += 2 + entries[i].plen + 16;  // u16 + path + u64 + u64
    }

    u64 content_start = 9ULL + (u64)total_meta;
    u64 cur_offset = content_start;
    for (size_t i = 0; i < count; i++) {
        entries[i].offset = cur_offset;
        cur_offset += entries[i].size;
    }

    // Write bundle
    FILE* bfp = fopen(bundle_path, "wb");
    if (!bfp) {
        fprintf(stderr, "Error: Cannot create bundle '%s'\n", bundle_path);
        // Cleanup
        for (size_t i = 0; i < count; i++) {
            free(entries[i].content);
            free(entries[i].path);
        }
        free(entries);
        return 1;
    }

    // Header
    fwrite("JSFS", 1, 4, bfp);
    fputc(1, bfp);
    write_u32_be(bfp, (u32)count);

    // Metadata
    for (size_t i = 0; i < count; i++) {
        write_u16_be(bfp, (u16)entries[i].plen);
        fwrite(entries[i].path, 1, entries[i].plen, bfp);
        write_u64_be(bfp, entries[i].size);
        write_u64_be(bfp, entries[i].offset);
    }

    // Contents
    for (size_t i = 0; i < count; i++) {
        fwrite(entries[i].content, 1, entries[i].size, bfp);
        free(entries[i].content);
        free(entries[i].path);
    }
    free(entries);

    fclose(bfp);
    printf("Bundled %zu files into '%s' (%llu bytes total)\n", count, bundle_path, content_start + (cur_offset - content_start));
    return 0;
}
