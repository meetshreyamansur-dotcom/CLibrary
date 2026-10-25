#ifndef CLIBRARY_LIBRARY_H
#define CLIBRARY_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define STORE_DIR "Store"
#define BOOKS_FILE STORE_DIR "/books.tsv"
#define LOANS_FILE STORE_DIR "/loans.tsv"
#define SEED_FILE "seed/books.tsv"
#define MAX_BOOKS 10000
#define MAX_LOANS 50000
#define LINE_SIZE 16384

typedef struct {
    int id;
    bool withdrawn;
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
    char patron[120];
    char checkout[11];
    char due[11];
    char checked_in[11];
    char notes[256];
} Loan;

typedef struct {
    Book books[MAX_BOOKS];
    Loan loans[MAX_LOANS];
    size_t book_count;
    size_t loan_count;
    int next_book_id;
    int next_loan_id;
} Library;

void input_text(const char *prompt, char *out, size_t capacity);
void input_required(const char *prompt, char *out, size_t capacity);
int input_choice(const char *prompt, int minimum, int maximum);
void input_date(const char *prompt, char out[11]);
void input_pause(void);
bool date_valid(const char *text);
long date_ordinal(const char *date);
void date_today(char out[11]);
bool text_equal_case_insensitive(const char *left, const char *right);
bool text_contains_case_insensitive(const char *text, const char *query);

void ui_banner(void);
void ui_dashboard(const Library *library);
void ui_menu(void);
bool ui_startup(Library *library);
void ui_run(Library *library);

bool storage_load(Library *library);
bool storage_save_books(const Library *library);
bool storage_save_loans(const Library *library);

Book *catalogue_find(Library *library, int id);
const Book *catalogue_find_const(const Library *library, int id);
bool catalogue_available(const Library *library, const Book *book);
void catalogue_list(const Library *library, bool include_withdrawn);
void catalogue_details(const Library *library);
void catalogue_print_record(const Library *library, const Book *book);
void catalogue_add(Library *library);
void catalogue_withdraw(Library *library);
void catalogue_remove(Library *library);

Loan *circulation_active_loan(Library *library, int book_id);
void circulation_checkout(Library *library);
void circulation_checkin(Library *library);

void search_catalogue(const Library *library);
bool seed_import(Library *library);

#endif
