#include "library.h"

#include <stdio.h>

void search_catalogue(const Library *library) {
    char query[256];
    int mode = input_choice("\nSearch type: 1) Exact match  2) Fuzzy/contains match\nChoice: ", 1, 2);
    input_required("Search title, author, publisher, year, ISBN, description, patron, and circulation dates: ",
                   query, sizeof query);
    bool any = false;
    for (size_t i = 0; i < library->book_count; i++) {
        const Book *book = &library->books[i];
        bool match = false;
        const char *book_fields[] = {
            book->title, book->author, book->publisher,
            book->year, book->isbn, book->description
        };
        for (size_t j = 0; j < sizeof book_fields / sizeof book_fields[0]; j++) {
            if (mode == 1 ? text_equal_case_insensitive(book_fields[j], query)
                          : text_contains_case_insensitive(book_fields[j], query)) match = true;
        }
        for (size_t j = 0; j < library->loan_count; j++) {
            const Loan *loan = &library->loans[j];
            if (loan->book_id != book->id) continue;
            const char *loan_fields[] = {
                loan->patron, loan->checkout, loan->due, loan->checked_in, loan->notes
            };
            for (size_t k = 0; k < sizeof loan_fields / sizeof loan_fields[0]; k++) {
                if (mode == 1 ? text_equal_case_insensitive(loan_fields[k], query)
                              : text_contains_case_insensitive(loan_fields[k], query)) match = true;
            }
        }
        if (match) {
            catalogue_print_record(library, book);
            any = true;
        }
    }
    if (!any) puts("\nNo matching titles found.");
    input_pause();
}
