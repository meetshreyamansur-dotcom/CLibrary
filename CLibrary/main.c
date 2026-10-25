#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STORE_DIR "Store"
#define BOOKS_FILE STORE_DIR "/books.tsv"
#define LOANS_FILE STORE_DIR "/loans.tsv"
#define MAX_BOOKS 10000
#define MAX_LOANS 50000
#define LINE_SIZE 16384

typedef struct {
    int id;
    bool archived;
    char added[11];
    char title[160];
    char author[120];
    char publisher[120];
    char year[5];
    char isbn[32];
    char description[512];
} Book;

typedef struct {
    int id;
    int book_id;
    char borrower[120];
    char checkout[11];
    char due[11];
    char returned[11];
    char notes[256];
} Loan;

static Book books[MAX_BOOKS];
static Loan loans[MAX_LOANS];
static size_t book_count;
static size_t loan_count;
static int next_book_id = 1;
static int next_loan_id = 1;

static void pause_for_enter(void) {
    char line[8];
    printf("\nPress Enter to continue...");
    (void)fgets(line, sizeof line, stdin);
}

static void read_text(const char *prompt, char *out, size_t capacity) {
    char line[LINE_SIZE];
    for (;;) {
        printf("%s", prompt);
        if (fgets(line, sizeof line, stdin) == NULL) {
            puts("\nEnd of input. Goodbye.");
            exit(EXIT_SUCCESS);
        }
        size_t length = strcspn(line, "\r\n");
        if (line[length] == '\0' && length == sizeof line - 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            puts("Input is too long. Please try again.");
            continue;
        }
        line[length] = '\0';
        while (length > 0 && isspace((unsigned char)line[length - 1])) {
            line[--length] = '\0';
        }
        char *start = line;
        while (isspace((unsigned char)*start)) start++;
        if (strlen(start) >= capacity) {
            printf("Please enter no more than %zu characters.\n", capacity - 1);
            continue;
        }
        memmove(out, start, strlen(start) + 1);
        return;
    }
}

static bool read_required(const char *prompt, char *out, size_t capacity) {
    do {
        read_text(prompt, out, capacity);
        if (out[0] == '\0') puts("This field is required.");
    } while (out[0] == '\0');
    return true;
}

static int read_menu_choice(const char *prompt, int minimum, int maximum) {
    char text[32];
    char *end;
    long value;
    for (;;) {
        read_text(prompt, text, sizeof text);
        errno = 0;
        value = strtol(text, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (text[0] != '\0' && *end == '\0' && errno == 0 &&
            value >= minimum && value <= maximum) {
            return (int)value;
        }
        printf("Enter a number from %d to %d.\n", minimum, maximum);
    }
}

static bool valid_date(const char *text) {
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    for (size_t i = 0; i < 10; i++) {
        if (i != 4 && i != 7 && !isdigit((unsigned char)text[i])) return false;
    }
    int year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 +
               (text[2] - '0') * 10 + text[3] - '0';
    int month = (text[5] - '0') * 10 + text[6] - '0';
    int day = (text[8] - '0') * 10 + text[9] - '0';
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1 || month < 1 || month > 12) return false;
    int maximum = days[month - 1];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) maximum++;
    return day >= 1 && day <= maximum;
}

static long date_ordinal(const char *date) {
    int y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
            (date[2] - '0') * 10 + date[3] - '0';
    int m = (date[5] - '0') * 10 + date[6] - '0';
    int d = (date[8] - '0') * 10 + date[9] - '0';
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153U * (unsigned)(m + (m > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)d - 1U;
    unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return era * 146097L + (long)doe;
}

static void read_date(const char *prompt, char out[11], bool optional) {
    for (;;) {
        read_text(prompt, out, 11);
        if (optional && out[0] == '\0') return;
        if (valid_date(out)) return;
        puts("Use a valid date in YYYY-MM-DD format.");
    }
}

static void today(char out[11]) {
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local == NULL || strftime(out, 11, "%Y-%m-%d", local) == 0) {
        fprintf(stderr, "Unable to determine today's date.\n");
        exit(EXIT_FAILURE);
    }
}

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
            int high, low;
            if (!isxdigit((unsigned char)read[1]) || !isxdigit((unsigned char)read[2])) return false;
            high = isdigit((unsigned char)read[1]) ? read[1] - '0' : toupper((unsigned char)read[1]) - 'A' + 10;
            low = isdigit((unsigned char)read[2]) ? read[2] - '0' : toupper((unsigned char)read[2]) - 'A' + 10;
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

static bool parse_int(const char *text, int *out) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || errno != 0 || value < 1 || value >= INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool copy_loaded(char *destination, size_t capacity, const char *source) {
    if (strlen(source) >= capacity) return false;
    strcpy(destination, source);
    return true;
}

static bool load_books(void) {
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
        char *f[9];
        int id;
        size_t n = split_fields(line, f, 9);
        if (n != 9 || !parse_int(f[0], &id) || (strcmp(f[1], "0") && strcmp(f[1], "1")) ||
            !valid_date(f[2]) || book_count == MAX_BOOKS) {
            fprintf(stderr, "%s:%zu: invalid or excessive book record.\n", BOOKS_FILE, line_number);
            fclose(file);
            return false;
        }
        Book *b = &books[book_count];
        b->id = id;
        b->archived = f[1][0] == '1';
        if (!copy_loaded(b->added, sizeof b->added, f[2]) ||
            !copy_loaded(b->title, sizeof b->title, f[3]) ||
            !copy_loaded(b->author, sizeof b->author, f[4]) ||
            !copy_loaded(b->publisher, sizeof b->publisher, f[5]) ||
            !copy_loaded(b->year, sizeof b->year, f[6]) ||
            !copy_loaded(b->isbn, sizeof b->isbn, f[7]) ||
            !copy_loaded(b->description, sizeof b->description, f[8])) {
            fprintf(stderr, "%s:%zu: field exceeds supported size.\n", BOOKS_FILE, line_number);
            fclose(file);
            return false;
        }
        book_count++;
        if (id >= next_book_id) next_book_id = id + 1;
    }
    bool ok = !ferror(file);
    if (!ok) fprintf(stderr, "Unable to read %s.\n", BOOKS_FILE);
    fclose(file);
    return ok;
}

static bool load_loans(void) {
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
        char *f[7];
        int id, book_id;
        size_t n = split_fields(line, f, 7);
        if (n != 7 || !parse_int(f[0], &id) || !parse_int(f[1], &book_id) ||
            !valid_date(f[3]) || !valid_date(f[4]) ||
            (f[5][0] != '\0' && !valid_date(f[5])) || loan_count == MAX_LOANS) {
            fprintf(stderr, "%s:%zu: invalid or excessive loan record.\n", LOANS_FILE, line_number);
            fclose(file);
            return false;
        }
        Loan *loan = &loans[loan_count];
        loan->id = id;
        loan->book_id = book_id;
        if (!copy_loaded(loan->borrower, sizeof loan->borrower, f[2]) ||
            !copy_loaded(loan->checkout, sizeof loan->checkout, f[3]) ||
            !copy_loaded(loan->due, sizeof loan->due, f[4]) ||
            !copy_loaded(loan->returned, sizeof loan->returned, f[5]) ||
            !copy_loaded(loan->notes, sizeof loan->notes, f[6])) {
            fprintf(stderr, "%s:%zu: field exceeds supported size.\n", LOANS_FILE, line_number);
            fclose(file);
            return false;
        }
        loan_count++;
        if (id >= next_loan_id) next_loan_id = id + 1;
    }
    bool ok = !ferror(file);
    if (!ok) fprintf(stderr, "Unable to read %s.\n", LOANS_FILE);
    fclose(file);
    return ok;
}

static bool save_books(void) {
    const char *temporary = BOOKS_FILE ".tmp";
    FILE *file = fopen(temporary, "w");
    if (file == NULL) {
        perror("Unable to save book catalogue");
        return false;
    }
    for (size_t i = 0; i < book_count; i++) {
        Book *b = &books[i];
        fprintf(file, "%d\t%d\t", b->id, b->archived ? 1 : 0);
        encode_field(file, b->added); fputc('\t', file);
        encode_field(file, b->title); fputc('\t', file);
        encode_field(file, b->author); fputc('\t', file);
        encode_field(file, b->publisher); fputc('\t', file);
        encode_field(file, b->year); fputc('\t', file);
        encode_field(file, b->isbn); fputc('\t', file);
        encode_field(file, b->description); fputc('\n', file);
    }
    bool ok = !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (ok && rename(temporary, BOOKS_FILE) == 0) return true;
    if (ok) perror("Unable to replace book catalogue");
    else perror("Unable to finish saving book catalogue");
    (void)unlink(temporary);
    return false;
}

static bool save_loans(void) {
    const char *temporary = LOANS_FILE ".tmp";
    FILE *file = fopen(temporary, "w");
    if (file == NULL) {
        perror("Unable to save lending history");
        return false;
    }
    for (size_t i = 0; i < loan_count; i++) {
        Loan *loan = &loans[i];
        fprintf(file, "%d\t%d\t", loan->id, loan->book_id);
        encode_field(file, loan->borrower); fputc('\t', file);
        encode_field(file, loan->checkout); fputc('\t', file);
        encode_field(file, loan->due); fputc('\t', file);
        encode_field(file, loan->returned); fputc('\t', file);
        encode_field(file, loan->notes); fputc('\n', file);
    }
    bool ok = !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (ok && rename(temporary, LOANS_FILE) == 0) return true;
    if (ok) perror("Unable to replace lending history");
    else perror("Unable to finish saving lending history");
    (void)unlink(temporary);
    return false;
}

static bool text_equals_case_insensitive(const char *left, const char *right) {
    while (*left && *right) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) return false;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static bool text_contains_case_insensitive(const char *text, const char *query) {
    if (*query == '\0') return true;
    for (; *text; text++) {
        const char *candidate = text;
        const char *needle = query;
        while (*candidate && *needle &&
               tolower((unsigned char)*candidate) == tolower((unsigned char)*needle)) {
            candidate++;
            needle++;
        }
        if (*needle == '\0') return true;
    }
    return false;
}

static Book *find_book(int id) {
    for (size_t i = 0; i < book_count; i++) if (books[i].id == id) return &books[i];
    return NULL;
}

static Loan *active_loan(int book_id) {
    for (size_t i = 0; i < loan_count; i++) {
        if (loans[i].book_id == book_id && loans[i].returned[0] == '\0') return &loans[i];
    }
    return NULL;
}

static bool is_available(const Book *book) {
    return !book->archived && active_loan(book->id) == NULL;
}

static void dashboard(void) {
    size_t catalogued = 0, available = 0, disbursed = 0, archived = 0;
    for (size_t i = 0; i < book_count; i++) {
        if (books[i].archived) {
            archived++;
        } else {
            catalogued++;
            if (active_loan(books[i].id) != NULL) disbursed++;
            else available++;
        }
    }
    printf("\n"
           "  LIBRARY OVERVIEW  |  Catalogue: %zu  |  Available: %zu  |  Disbursed: %zu  |  Archived: %zu\n"
           "  ------------------------------------------------------------------------------------------\n",
           catalogued, available, disbursed, archived);
}

static void print_book(const Book *book) {
    Loan *current = active_loan(book->id);
    printf("\nBook ID:       %d\n", book->id);
    printf("Title:         %s\n", book->title);
    printf("Author:        %s\n", book->author);
    printf("Publisher:     %s\n", book->publisher[0] ? book->publisher : "Not recorded");
    printf("Year:          %s\n", book->year[0] ? book->year : "Not recorded");
    printf("ISBN:          %s\n", book->isbn[0] ? book->isbn : "Not recorded");
    printf("Added:         %s\n", book->added);
    printf("Description:   %s\n", book->description[0] ? book->description : "No description recorded");
    printf("Status:        %s\n", book->archived ? "Archived" : current ? "Disbursed" : "Available in library");
    if (current != NULL) {
        char today_date[11];
        today(today_date);
        long elapsed = date_ordinal(today_date) - date_ordinal(current->checkout);
        printf("Possessing:    %s\n", current->borrower);
        printf("Checked out:   %s (%ld day%s ago)\n", current->checkout, elapsed, elapsed == 1 ? "" : "s");
        printf("Due date:      %s%s\n", current->due,
               strcmp(today_date, current->due) > 0 ? "  [OVERDUE]" : "");
        printf("Loan notes:    %s\n", current->notes[0] ? current->notes : "None");
    }
    bool has_history = false;
    for (size_t i = 0; i < loan_count; i++) {
        Loan *loan = &loans[i];
        if (loan->book_id != book->id) continue;
        if (!has_history) {
            puts("Lending history:");
            has_history = true;
        }
        printf("  #%d  %s  |  Out: %s  |  Due: %s  |  Returned: %s\n",
               loan->id, loan->borrower, loan->checkout, loan->due,
               loan->returned[0] ? loan->returned : "Still out");
    }
}

static void list_books(bool include_archived) {
    bool any = false;
    puts("\nID    STATUS      TITLE                                      AUTHOR");
    puts("----- ----------- ------------------------------------------ ------------------------------");
    for (size_t i = 0; i < book_count; i++) {
        Book *b = &books[i];
        if (!include_archived && b->archived) continue;
        Loan *loan = active_loan(b->id);
        printf("%-5d %-11s %-42.42s %-30.30s\n", b->id,
               b->archived ? "ARCHIVED" : loan ? "DISBURSED" : "AVAILABLE",
               b->title, b->author);
        any = true;
    }
    if (!any) puts("No books to display.");
}

static void add_book(void) {
    if (book_count == MAX_BOOKS || next_book_id == INT_MAX) {
        puts("The catalogue has reached its capacity.");
        pause_for_enter();
        return;
    }
    Book book = {0};
    book.id = next_book_id;
    today(book.added);
    puts("\nADD A BOOK");
    read_required("Title: ", book.title, sizeof book.title);
    read_required("Author: ", book.author, sizeof book.author);
    read_text("Publisher (optional): ", book.publisher, sizeof book.publisher);
    read_text("Publication year (optional): ", book.year, sizeof book.year);
    read_text("ISBN (optional): ", book.isbn, sizeof book.isbn);
    read_text("Description (optional): ", book.description, sizeof book.description);
    books[book_count++] = book;
    next_book_id++;
    if (!save_books()) {
        book_count--;
        next_book_id--;
        pause_for_enter();
        return;
    }
    printf("Added \"%s\" to the catalogue (book ID %d).\n", book.title, book.id);
    pause_for_enter();
}

static void search_books(void) {
    char query[256];
    int mode = read_menu_choice("\nSearch type: 1) Exact match  2) Fuzzy/contains match\nChoice: ", 1, 2);
    read_required("Search across title, author, publisher, year, ISBN, description, borrower, and dates: ",
                  query, sizeof query);
    bool any = false;
    for (size_t i = 0; i < book_count; i++) {
        Book *b = &books[i];
        bool match = false;
        const char *fields[] = {b->title, b->author, b->publisher, b->year, b->isbn, b->description};
        for (size_t j = 0; j < sizeof fields / sizeof fields[0]; j++) {
            if (mode == 1 ? text_equals_case_insensitive(fields[j], query)
                          : text_contains_case_insensitive(fields[j], query)) {
                match = true;
            }
        }
        for (size_t j = 0; j < loan_count; j++) {
            Loan *loan = &loans[j];
            if (loan->book_id != b->id) continue;
            const char *loan_fields[] = {loan->borrower, loan->checkout, loan->due, loan->returned, loan->notes};
            for (size_t k = 0; k < sizeof loan_fields / sizeof loan_fields[0]; k++) {
                if (mode == 1 ? text_equals_case_insensitive(loan_fields[k], query)
                              : text_contains_case_insensitive(loan_fields[k], query)) match = true;
            }
        }
        if (match) {
            print_book(b);
            any = true;
        }
    }
    if (!any) puts("\nNo matching books found.");
    pause_for_enter();
}

static void archive_book(void) {
    int id = read_menu_choice("\nBook ID to archive (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = find_book(id);
    if (book == NULL || book->archived) {
        puts("That active book ID was not found.");
        pause_for_enter();
        return;
    }
    if (active_loan(id) != NULL) {
        puts("This book is currently disbursed. Record its return before archiving it.");
        pause_for_enter();
        return;
    }
    book->archived = true;
    if (!save_books()) book->archived = false;
    else printf("Book %d has been archived. Its catalogue and lending history are retained.\n", id);
    pause_for_enter();
}

static void remove_book(void) {
    int id = read_menu_choice("\nBook ID to permanently remove (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = find_book(id);
    if (book == NULL) {
        puts("That book ID was not found.");
        pause_for_enter();
        return;
    }
    if (active_loan(id) != NULL) {
        puts("This book is currently disbursed. Record its return before removing it.");
        pause_for_enter();
        return;
    }
    for (size_t i = 0; i < loan_count; i++) {
        if (loans[i].book_id == id) {
            puts("This book has lending history and cannot be permanently removed.");
            puts("Archive it instead to retain the book record and lending history.");
            pause_for_enter();
            return;
        }
    }
    char confirmation[16];
    printf("Permanently remove \"%s\" (ID %d)? This cannot be undone. (y/N): ", book->title, id);
    read_text("", confirmation, sizeof confirmation);
    if (!text_equals_case_insensitive(confirmation, "y") &&
        !text_equals_case_insensitive(confirmation, "yes")) {
        puts("Removal cancelled.");
        pause_for_enter();
        return;
    }

    size_t index = (size_t)(book - books);
    Book removed = books[index];
    for (size_t i = index; i + 1 < book_count; i++) books[i] = books[i + 1];
    book_count--;
    if (!save_books()) {
        for (size_t i = book_count; i > index; i--) books[i] = books[i - 1];
        books[index] = removed;
        book_count++;
    } else {
        printf("Book %d was permanently removed.\n", id);
    }
    pause_for_enter();
}

static void lend_book(void) {
    if (loan_count == MAX_LOANS || next_loan_id == INT_MAX) {
        puts("The lending history has reached its capacity.");
        pause_for_enter();
        return;
    }
    int id = read_menu_choice("\nBook ID to lend (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = find_book(id);
    if (book == NULL || !is_available(book)) {
        puts("That book is unavailable. Check its ID and current status.");
        pause_for_enter();
        return;
    }
    Loan loan = {0};
    loan.id = next_loan_id;
    loan.book_id = id;
    today(loan.checkout);
    read_required("Borrower / receiver: ", loan.borrower, sizeof loan.borrower);
    read_date("Due date (YYYY-MM-DD): ", loan.due, false);
    if (date_ordinal(loan.due) < date_ordinal(loan.checkout)) {
        puts("Due date cannot be earlier than today.");
        pause_for_enter();
        return;
    }
    read_text("Loan notes (optional): ", loan.notes, sizeof loan.notes);
    loans[loan_count++] = loan;
    next_loan_id++;
    if (!save_loans()) {
        loan_count--;
        next_loan_id--;
        pause_for_enter();
        return;
    }
    printf("Book \"%s\" has been recorded as disbursed to %s.\n", book->title, loan.borrower);
    pause_for_enter();
}

static void return_book(void) {
    int id = read_menu_choice("\nBook ID being returned (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Loan *loan = active_loan(id);
    if (loan == NULL) {
        puts("No active loan was found for that book ID.");
        pause_for_enter();
        return;
    }
    char returned[11];
    today(returned);
    strcpy(loan->returned, returned);
    if (!save_loans()) loan->returned[0] = '\0';
    else printf("Return recorded for book %d from %s.\n", id, loan->borrower);
    pause_for_enter();
}

static void show_book_details(void) {
    int id = read_menu_choice("\nBook ID to view (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = find_book(id);
    if (book == NULL) puts("Book ID not found.");
    else print_book(book);
    pause_for_enter();
}

static bool ensure_store(void) {
    struct stat info;
    if (stat(STORE_DIR, &info) == 0) {
        if (!S_ISDIR(info.st_mode)) {
            fprintf(stderr, "%s exists but is not a directory.\n", STORE_DIR);
            return false;
        }
        return true;
    }
    if (errno != ENOENT || mkdir(STORE_DIR, 0755) != 0) {
        perror("Unable to create Store directory");
        return false;
    }
    return true;
}

int main(void) {
    if (!ensure_store() || !load_books() || !load_loans()) {
        fprintf(stderr, "Library data could not be loaded. No changes have been made.\n");
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < loan_count; i++) {
        if (find_book(loans[i].book_id) == NULL) {
            fprintf(stderr, "%s contains a loan for missing book ID %d.\n",
                    LOANS_FILE, loans[i].book_id);
            return EXIT_FAILURE;
        }
    }

    for (;;) {
        dashboard();
        puts("  LIBRARY MANAGEMENT");
        puts("  1. View book catalogue");
        puts("  2. Search catalogue (exact or fuzzy)");
        puts("  3. View book details and lending history");
        puts("  4. Add a book");
        puts("  5. Lend a book");
        puts("  6. Record a book return");
        puts("  7. Archive a book");
        puts("  8. Permanently remove a book (no lending history)");
        puts("  9. View archived books");
        puts("  0. Exit");
        int choice = read_menu_choice("\nSelect an option: ", 0, 9);
        switch (choice) {
            case 1: list_books(false); pause_for_enter(); break;
            case 2: search_books(); break;
            case 3: show_book_details(); break;
            case 4: add_book(); break;
            case 5: lend_book(); break;
            case 6: return_book(); break;
            case 7: archive_book(); break;
            case 8: remove_book(); break;
            case 9: list_books(true); pause_for_enter(); break;
            case 0: puts("\nLibrary data saved. Goodbye."); return EXIT_SUCCESS;
        }
    }
}
