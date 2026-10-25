#include "library.h"
#include "storage.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define MINIMUM_SEED_TITLES 50

static bool seed_split(char *line, char **fields, size_t field_count) {
    size_t current = 0;
    char *start = line;
    for (char *p = line;; p++) {
        if (*p == '\t' || *p == '\0') {
            if (current == field_count) return false;
            fields[current++] = start;
            if (*p == '\0') break;
            *p = '\0';
            start = p + 1;
        }
    }
    return current == field_count;
}

static bool copy_seed_field(char *destination, size_t capacity, const char *source) {
    if (strlen(source) >= capacity) return false;
    strcpy(destination, source);
    return true;
}

static bool same_title(const Book *left, const Book *right) {
    return text_equal_case_insensitive(left->title, right->title) &&
           text_equal_case_insensitive(left->author, right->author);
}

bool seed_import(Library *library) {
    FILE *file = fopen(SEED_FILE, "r");
    if (file == NULL) {
        perror("Unable to open bundled sample catalogue");
        return false;
    }

    static Book seed_books[256];
    size_t seed_count = 0;
    char line[LINE_SIZE];
    size_t line_number = 0;
    bool valid = true;
    while (fgets(line, sizeof line, file) != NULL) {
        line_number++;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "%s:%zu: sample record is too long.\n", SEED_FILE, line_number);
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;
        if (seed_count == sizeof seed_books / sizeof seed_books[0]) {
            fprintf(stderr, "%s contains more sample titles than supported.\n", SEED_FILE);
            valid = false;
            break;
        }
        char *fields[6];
        if (!seed_split(line, fields, 6)) {
            fprintf(stderr, "%s:%zu: expected six tab-separated fields.\n", SEED_FILE, line_number);
            valid = false;
            break;
        }
        Book *book = &seed_books[seed_count];
        if (fields[0][0] == '\0' || fields[1][0] == '\0' ||
            !copy_seed_field(book->title, sizeof book->title, fields[0]) ||
            !copy_seed_field(book->author, sizeof book->author, fields[1]) ||
            !copy_seed_field(book->publisher, sizeof book->publisher, fields[2]) ||
            !copy_seed_field(book->year, sizeof book->year, fields[3]) ||
            !copy_seed_field(book->isbn, sizeof book->isbn, fields[4]) ||
            !copy_seed_field(book->description, sizeof book->description, fields[5])) {
            fprintf(stderr, "%s:%zu: invalid or oversized sample field.\n", SEED_FILE, line_number);
            valid = false;
            break;
        }
        seed_count++;
    }
    if (ferror(file)) {
        fprintf(stderr, "Unable to read %s.\n", SEED_FILE);
        valid = false;
    }
    fclose(file);
    if (!valid) return false;
    if (seed_count < MINIMUM_SEED_TITLES) {
        fprintf(stderr, "%s must contain at least %d sample titles; found %zu.\n",
                SEED_FILE, MINIMUM_SEED_TITLES, seed_count);
        return false;
    }

    size_t original_count = library->book_count;
    int original_next_id = library->next_book_id;
    size_t imported = 0;
    size_t skipped = 0;
    char added_date[11];
    date_today(added_date);
    for (size_t i = 0; i < seed_count; i++) {
        bool duplicate = false;
        for (size_t j = 0; j < library->book_count; j++) {
            if (same_title(&seed_books[i], &library->books[j])) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            skipped++;
            continue;
        }
        if (library->book_count == MAX_BOOKS || library->next_book_id == INT_MAX) {
            puts("The catalogue has insufficient capacity for the remaining sample titles.");
            break;
        }
        Book *destination = &library->books[library->book_count++];
        *destination = seed_books[i];
        destination->id = library->next_book_id++;
        strcpy(destination->added, added_date);
        imported++;
    }
    if (imported == 0) {
        puts("No sample titles were added; the bundled titles are already in the catalogue.");
        return true;
    }
    if (!storage_save_books(library)) {
        library->book_count = original_count;
        library->next_book_id = original_next_id;
        return false;
    }
    printf("Sample catalogue loaded: %zu title%s added, %zu duplicate%s skipped.\n",
           imported, imported == 1 ? "" : "s", skipped, skipped == 1 ? "" : "s");
    return true;
}
