#include <base/io.h>
#include <base/arena.h>
#include <base/scratch.h>
#include <base/buddy.h>
#include <base/base_string.h>
#include <base/hashtable.h>
#include <base/vector.h>
#include <base/wasi.h>
#include <base/mem.h>
#include <base/numconv.h>
#include <base/format.h>
#include <base/base_io.h>
#include <base/exit.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_MAGENTA "\033[35m"

// Word frequency entry
typedef struct {
    string word;
    uint64_t count;
} WordEntry;

// Define vector type for WordEntry
DEFINE_VECTOR_FOR_TYPE(WordEntry, WordEntryVec)

// Define hashtable for word counting: string -> uint64_t
// Need to define hash and equality functions
#define WordFreqTable_HASH(key) str_hash(key)
#define WordFreqTable_EQUAL(a, b) str_eq(a, b)
DEFINE_HASHTABLE_FOR_TYPES(string, uint64_t, WordFreqTable)

// Helper: check if character is alphanumeric
static bool is_alnum(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

// Helper: convert to lowercase
static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

// Tokenize text and count word frequencies
static void count_words(Arena *arena, string text, WordFreqTable *table) {
    println(str_lit("count_words: Starting, text size = {}"), (int64_t)text.size);

    size_t i = 0;
    size_t word_count = 0;
    while (i < text.size) {
        // Skip non-alphanumeric characters
        while (i < text.size && !is_alnum(text.str[i])) {
            i++;
        }

        if (i >= text.size) break;

        // Found start of a word
        size_t start = i;
        while (i < text.size && is_alnum(text.str[i])) {
            i++;
        }

        // Extract and normalize word to lowercase
        size_t word_len = i - start;
        char *word_buf = arena_alloc_array(arena, char, word_len);
        for (size_t j = 0; j < word_len; j++) {
            word_buf[j] = to_lower(text.str[start + j]);
        }
        string word = str_from_cstr_len_view(word_buf, word_len);

        // Update count in hashtable
        uint64_t *count_ptr = WordFreqTable_get(table, word);
        uint64_t new_count = count_ptr ? (*count_ptr + 1) : 1;
        WordFreqTable_insert(arena, table, word, new_count);

        word_count++;
        if (word_count % 10 == 0) {
            println(str_lit("Processed {} words..."), (int64_t)word_count);
        }
    }
    println(str_lit("Total words processed: {}"), (int64_t)word_count);
}

// Comparison function for sorting (descending by count)
static int compare_entries(const void *a, const void *b) {
    const WordEntry *ea = (const WordEntry *)a;
    const WordEntry *eb = (const WordEntry *)b;
    if (ea->count > eb->count) return -1;
    if (ea->count < eb->count) return 1;
    return 0;
}

// Simple qsort implementation (since we can't use stdlib)
static void swap_entries(WordEntry *a, WordEntry *b) {
    WordEntry temp = *a;
    *a = *b;
    *b = temp;
}

static void quicksort(WordEntry *arr, int64_t low, int64_t high) {
    if (low < high) {
        // Partition
        WordEntry pivot = arr[high];
        int64_t i = low - 1;

        for (int64_t j = low; j < high; j++) {
            if (compare_entries(&arr[j], &pivot) < 0) {
                i++;
                swap_entries(&arr[i], &arr[j]);
            }
        }
        swap_entries(&arr[i + 1], &arr[high]);
        int64_t pi = i + 1;

        // Recursively sort
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

// Print usage
static void print_usage(const char *prog_name) {
    println(str_lit("Usage: {} <filename> [top_n] [bottom_n]"), str_from_cstr_view((char *)prog_name));
    println(str_lit("  filename  - text file to analyze"));
    println(str_lit("  top_n     - number of most frequent words (default: 20)"));
    println(str_lit("  bottom_n  - number of least frequent words (default: 10)"));
}

// Parse integer from string
static bool parse_int(string s, int64_t *result) {
    *result = 0;
    if (s.size == 0) return false;

    for (size_t i = 0; i < s.size; i++) {
        if (s.str[i] < '0' || s.str[i] > '9') {
            return false;
        }
        *result = *result * 10 + (s.str[i] - '0');
    }
    return true;
}

int main(void) {
    // Get command line arguments
    Scratch scratch = scratch_begin();

    size_t argc;
    size_t argv_buf_size;
    if (wasi_args_sizes_get(&argc, &argv_buf_size) != 0) {
        println(str_lit("Error: Failed to get argument sizes"));
        return 1;
    }

    char **argv = (char **)arena_alloc(scratch.arena, argc * sizeof(char*));
    char *argv_buf = (char *)arena_alloc(scratch.arena, argv_buf_size);
    if (wasi_args_get(argv, argv_buf) != 0) {
        println(str_lit("Error: Failed to get arguments"));
        buddy_free(argv);
        buddy_free(argv_buf);
        return 1;
    }

    // Parse arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    string filename = str_from_cstr_view(argv[1]);
    int64_t top_n = 20;
    int64_t bottom_n = 10;

    if (argc >= 3) {
        if (!parse_int(str_from_cstr_view(argv[2]), &top_n)) {
            println(str_lit("Error: Invalid top_n value"));
            return 1;
        }
    }

    if (argc >= 4) {
        if (!parse_int(str_from_cstr_view(argv[3]), &bottom_n)) {
            println(str_lit("Error: Invalid bottom_n value"));
            return 1;
        }
    }

    // Now create an arena for the rest of the work
    Arena *arena = arena_new(1024 * 1024); // 1MB initial

    println(str_lit("Reading file..."));

    // Read file
    string text;
    if (!read_file(arena, filename, &text)) {
        println(str_lit("Error: Cannot read file '{}'"), filename);
        return 1;
    }

    println(str_lit("File read successfully, size: {}"), (int64_t)text.size);

    // Initialize hashtable
    WordFreqTable table;
    WordFreqTable_init(arena, &table, 1024);

    println(str_lit("Counting words..."));

    // Count words
    count_words(arena, text, &table);

    println(str_lit("Finished counting words"));

    if (table.size == 0) {
        println(str_lit("No words found in file"));
        return 0;
    }

    // Convert hashtable to vector for sorting
    WordEntryVec entries;
    WordEntryVec_reserve(arena, &entries, table.size);

    uint64_t total_count = 0;
    for (size_t i = 0; i < table.num_buckets; i++) {
        if (table.buckets[i].occupied) {
            WordEntry entry;
            entry.word = table.buckets[i].key;
            entry.count = table.buckets[i].value;
            WordEntryVec_push_back(arena, &entries, entry);
            total_count += entry.count;
        }
    }

    // Sort by frequency (descending)
    quicksort(entries.data, 0, (int64_t)entries.size - 1);

    // Print header
    println(str_lit("{}=== Word Frequency Analysis ==={}"), str_lit(COLOR_BOLD COLOR_CYAN), str_lit(COLOR_RESET));
    println(str_lit("Total words: {}"), (int64_t)total_count);
    println(str_lit("Unique words: {}"), (int64_t)entries.size);
    println(str_lit(""));

    // Print top N
    println(str_lit("{}=== Top {} Most Frequent Words ==={}"), str_lit(COLOR_BOLD COLOR_GREEN), top_n, str_lit(COLOR_RESET));
    int64_t top_limit = top_n;
    if (top_limit > (int64_t)entries.size) {
        top_limit = (int64_t)entries.size;
    }

    for (int64_t i = 0; i < top_limit; i++) {
        WordEntry *e = &entries.data[i];
        double percentage = (double)e->count * 100.0 / (double)total_count;

        // Format percentage with 2 decimal places
        char pct_buf[32];
        size_t pct_len = double_to_str(percentage, pct_buf, 2);

        println(str_lit("{}{:>3}. {:<20} {:>6}  {}%{}{}"),
                str_lit(COLOR_GREEN),
                i + 1,
                e->word,
                (int64_t)e->count,
                str_lit(COLOR_YELLOW),
                str_from_cstr_len_view(pct_buf, pct_len),
                str_lit(COLOR_RESET));
    }

    // Print bottom N
    println(str_lit(""));
    println(str_lit("{}=== Bottom {} Least Frequent Words ==={}"), str_lit(COLOR_BOLD COLOR_RED), bottom_n, str_lit(COLOR_RESET));

    int64_t bottom_start = (int64_t)entries.size - bottom_n;
    if (bottom_start < 0) bottom_start = 0;
    if (bottom_start < top_limit) bottom_start = top_limit; // Don't overlap with top

    for (int64_t i = bottom_start; i < (int64_t)entries.size; i++) {
        WordEntry *e = &entries.data[i];
        double percentage = (double)e->count * 100.0 / (double)total_count;

        char pct_buf[32];
        size_t pct_len = double_to_str(percentage, pct_buf, 2);

        println(str_lit("{}{:>3}. {:<20} {:>6}  {}%{}{}"),
                str_lit(COLOR_RED),
                i + 1,
                e->word,
                (int64_t)e->count,
                str_lit(COLOR_YELLOW),
                str_from_cstr_len_view(pct_buf, pct_len),
                str_lit(COLOR_RESET));
    }

    arena_free(arena);
    buddy_free(argv);
    buddy_free(argv_buf);
    return 0;
}
