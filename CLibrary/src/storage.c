#define _POSIX_C_SOURCE 200809L

#include "storage.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void encode_field(FILE *file, const char *value) {
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '%' || *p == '\t' || *p == '\n' || *p == '\r') {
            fprintf(file, "%%%02X", *p);
        } else {
            fputc(*p, file);
        }
    }
}

static bool decode_field(char *value) {
    char *read = value;
    char *write = value;
    while (*read) {
        if (*read == '%') {
            if (!isxdigit((unsigned char)read[1]) || !isxdigit((unsigned char)read[2])) return false;
            int high = isdigit((unsigned char)read[1]) ? read[1] - '0'
                : toupper((unsigned char)read[1]) - 'A' + 10;
            int low = isdigit((unsigned char)read[2]) ? read[2] - '0'
                : toupper((unsigned char)read[2]) - 'A' + 10;
            int decoded = high * 16 + low;
            if (decoded == 0) return false;
            *write++ = (char)decoded;
            read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
    return true;
}

static size_t split_fields(char *line, char **fields, size_t capacity) {
    size_t count = 0;
    char *start = line;
    for (char *p = line;; p++) {
        if (*p == '\t' || *p == '\0') {
            if (count == capacity) return capacity + 1;
            fields[count++] = start;
            if (*p == '\0') break;
            *p = '\0';
            start = p + 1;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (!decode_field(fields[i])) return capacity + 1;
    }
    return count;
}

static bool parse_id(const char *text, int *out) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || errno != 0 || value < 1 || value >= INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool copy_field(char *destination, size_t capacity, const char *source) {
    if (strlen(source) >= capacity) return false;
    strcpy(destination, source);
    return true;
}

static bool ensure_store(void) {
    struct stat info;
    if (stat(STORE_DIR, &info) == 0) {
        if (S_ISDIR(info.st_mode)) return true;
        fprintf(stderr, "%s exists but is not a directory.\n", STORE_DIR);
        return false;
    }
    if (errno != ENOENT || mkdir(STORE_DIR, 0755) != 0) {
        perror("Unable to create Store directory");
        return false;
    }
    return true;
}

static bool load_books(Library *library) {
    FILE *file = fopen(BOOKS_FILE, "r");
    if (file == NULL) return errno == ENOENT;
    char line[LINE_SIZE];
    size_t line_number = 0;
    while (fgets(line, sizeof line, file) != NULL) {
        line_number++;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "%s:%zu: record exceeds maximum line length.\n", BOOKS_FILE, line_number);
            fclose(file);
            return false;
        }
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[9];
        int id;
        size_t count = split_fields(line, fields, 9);
        if (count != 9 || !parse_id(fields[0], &id) ||
            (strcmp(fields[1], "0") && strcmp(fields[1], "1")) ||
            !date_valid(fields[2]) || library->book_count == MAX_BOOKS) {
            fprintf(stderr, "%s:%zu: invalid or excessive title record.\n", BOOKS_FILE, line_number);
            fclose(file);
            return false;
        }
        Book *book = &library->books[library->book_count];
        book->id = id;
        book->withdrawn = fields[1][0] == '1';
        if (!copy_field(book->added, sizeof book->added, fields[2]) ||
            !copy_field(book->title, sizeof book->title, fields[3]) ||
            !copy_field(book->author, sizeof book->author, fields[4]) ||
            !copy_field(book->publisher, sizeof book->publisher, fields[5]) ||
            !copy_field(book->year, sizeof book->year, fields[6]) ||
            !copy_field(book->isbn, sizeof book->isbn, fields[7]) ||
            !copy_field(book->description, sizeof book->description, fields[8])) {
            fprintf(stderr, "%s:%zu: field exceeds supported size.\n", BOOKS_FILE, line_number);
            fclose(file);
            return false;
        }
        library->book_count++;
        if (id >= library->next_book_id) library->next_book_id = id + 1;
    }
    bool ok = !ferror(file);
    if (!ok) fprintf(stderr, "Unable to read %s.\n", BOOKS_FILE);
    fclose(file);
    return ok;
}

static bool load_loans(Library *library) {
    FILE *file = fopen(LOANS_FILE, "r");
    if (file == NULL) return errno == ENOENT;
    char line[LINE_SIZE];
    size_t line_number = 0;
    while (fgets(line, sizeof line, file) != NULL) {
        line_number++;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "%s:%zu: record exceeds maximum line length.\n", LOANS_FILE, line_number);
            fclose(file);
            return false;
        }
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[7];
        int id, book_id;
        size_t count = split_fields(line, fields, 7);
        if (count != 7 || !parse_id(fields[0], &id) || !parse_id(fields[1], &book_id) ||
            !date_valid(fields[3]) || !date_valid(fields[4]) ||
            (fields[5][0] != '\0' && !date_valid(fields[5])) ||
            library->loan_count == MAX_LOANS) {
            fprintf(stderr, "%s:%zu: invalid or excessive circulation record.\n", LOANS_FILE, line_number);
            fclose(file);
            return false;
        }
        Loan *loan = &library->loans[library->loan_count];
        loan->id = id;
        loan->book_id = book_id;
        if (!copy_field(loan->patron, sizeof loan->patron, fields[2]) ||
            !copy_field(loan->checkout, sizeof loan->checkout, fields[3]) ||
            !copy_field(loan->due, sizeof loan->due, fields[4]) ||
            !copy_field(loan->checked_in, sizeof loan->checked_in, fields[5]) ||
            !copy_field(loan->notes, sizeof loan->notes, fields[6])) {
            fprintf(stderr, "%s:%zu: field exceeds supported size.\n", LOANS_FILE, line_number);
            fclose(file);
            return false;
        }
        library->loan_count++;
        if (id >= library->next_loan_id) library->next_loan_id = id + 1;
    }
    bool ok = !ferror(file);
    if (!ok) fprintf(stderr, "Unable to read %s.\n", LOANS_FILE);
    fclose(file);
    return ok;
}

bool storage_load(Library *library) {
    library->next_book_id = 1;
    library->next_loan_id = 1;
    if (!ensure_store() || !load_books(library) || !load_loans(library)) return false;
    for (size_t i = 0; i < library->loan_count; i++) {
        bool found = false;
        for (size_t j = 0; j < library->book_count; j++) {
            if (library->books[j].id == library->loans[i].book_id) found = true;
        }
        if (!found) {
            fprintf(stderr, "%s contains a loan for missing title ID %d.\n",
                    LOANS_FILE, library->loans[i].book_id);
            return false;
        }
    }
    return true;
}

static bool finish_save(FILE *file, const char *temporary, const char *destination,
                        const char *description) {
    bool ok = !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (ok && rename(temporary, destination) == 0) return true;
    if (ok) perror(description);
    else perror("Unable to finish writing data file");
    (void)unlink(temporary);
    return false;
}

bool storage_save_books(const Library *library) {
    const char *temporary = BOOKS_FILE ".tmp";
    FILE *file = fopen(temporary, "w");
    if (file == NULL) {
        perror("Unable to save catalogue");
        return false;
    }
    for (size_t i = 0; i < library->book_count; i++) {
        const Book *book = &library->books[i];
        fprintf(file, "%d\t%d\t", book->id, book->withdrawn ? 1 : 0);
        encode_field(file, book->added); fputc('\t', file);
        encode_field(file, book->title); fputc('\t', file);
        encode_field(file, book->author); fputc('\t', file);
        encode_field(file, book->publisher); fputc('\t', file);
        encode_field(file, book->year); fputc('\t', file);
        encode_field(file, book->isbn); fputc('\t', file);
        encode_field(file, book->description); fputc('\n', file);
    }
    return finish_save(file, temporary, BOOKS_FILE, "Unable to replace catalogue");
}

bool storage_save_loans(const Library *library) {
    const char *temporary = LOANS_FILE ".tmp";
    FILE *file = fopen(temporary, "w");
    if (file == NULL) {
        perror("Unable to save circulation history");
        return false;
    }
    for (size_t i = 0; i < library->loan_count; i++) {
        const Loan *loan = &library->loans[i];
        fprintf(file, "%d\t%d\t", loan->id, loan->book_id);
        encode_field(file, loan->patron); fputc('\t', file);
        encode_field(file, loan->checkout); fputc('\t', file);
        encode_field(file, loan->due); fputc('\t', file);
        encode_field(file, loan->checked_in); fputc('\t', file);
        encode_field(file, loan->notes); fputc('\n', file);
    }
    return finish_save(file, temporary, LOANS_FILE, "Unable to replace circulation history");
}
