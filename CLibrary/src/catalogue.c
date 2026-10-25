#include "library.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

Book *catalogue_find(Library *library, int id) {
    for (size_t i = 0; i < library->book_count; i++) {
        if (library->books[i].id == id) return &library->books[i];
    }
    return NULL;
}

const Book *catalogue_find_const(const Library *library, int id) {
    for (size_t i = 0; i < library->book_count; i++) {
        if (library->books[i].id == id) return &library->books[i];
    }
    return NULL;
}

static bool has_active_loan(const Library *library, int book_id) {
    for (size_t i = 0; i < library->loan_count; i++) {
        const Loan *loan = &library->loans[i];
        if (loan->book_id == book_id && loan->checked_in[0] == '\0') return true;
    }
    return false;
}

bool catalogue_available(const Library *library, const Book *book) {
    return !book->withdrawn && !has_active_loan(library, book->id);
}

void catalogue_print_record(const Library *library, const Book *book) {
    const Loan *current = NULL;
    for (size_t i = 0; i < library->loan_count; i++) {
        const Loan *loan = &library->loans[i];
        if (loan->book_id == book->id && loan->checked_in[0] == '\0') {
            current = loan;
            break;
        }
    }
    printf("\nTitle ID:      %d\n", book->id);
    printf("Title:         %s\n", book->title);
    printf("Author:        %s\n", book->author);
    printf("Publisher:     %s\n", book->publisher[0] ? book->publisher : "Not recorded");
    printf("Year:          %s\n", book->year[0] ? book->year : "Not recorded");
    printf("ISBN:          %s\n", book->isbn[0] ? book->isbn : "Not recorded");
    printf("Added:         %s\n", book->added);
    printf("Description:   %s\n", book->description[0] ? book->description : "No description recorded");
    printf("Circulation:   %s\n", book->withdrawn ? "Withdrawn from circulation"
                                                   : current ? "On loan" : "Available");
    if (current != NULL) {
        char today[11];
        date_today(today);
        long elapsed = date_ordinal(today) - date_ordinal(current->checkout);
        printf("Patron:        %s\n", current->patron);
        printf("Checked out:   %s (%ld day%s ago)\n", current->checkout, elapsed, elapsed == 1 ? "" : "s");
        printf("Due date:      %s%s\n", current->due, strcmp(today, current->due) > 0 ? "  [OVERDUE]" : "");
        printf("Loan notes:    %s\n", current->notes[0] ? current->notes : "None");
    }
    bool has_history = false;
    for (size_t i = 0; i < library->loan_count; i++) {
        const Loan *loan = &library->loans[i];
        if (loan->book_id != book->id) continue;
        if (!has_history) {
            puts("Circulation history:");
            has_history = true;
        }
        printf("  #%d  %s  |  Checkout: %s  |  Due: %s  |  Check-in: %s\n",
               loan->id, loan->patron, loan->checkout, loan->due,
               loan->checked_in[0] ? loan->checked_in : "Not yet checked in");
    }
}

void catalogue_list(const Library *library, bool include_withdrawn) {
    bool any = false;
    puts("\nID    STATUS      TITLE                                      AUTHOR");
    puts("----- ----------- ------------------------------------------ ------------------------------");
    for (size_t i = 0; i < library->book_count; i++) {
        const Book *book = &library->books[i];
        if (!include_withdrawn && book->withdrawn) continue;
        const char *status = book->withdrawn ? "WITHDRAWN"
            : catalogue_available(library, book) ? "AVAILABLE" : "ON LOAN";
        printf("%-5d %-11s %-42.42s %-30.30s\n",
               book->id, status, book->title, book->author);
        any = true;
    }
    if (!any) puts("No titles to display.");
}

void catalogue_details(const Library *library) {
    int id = input_choice("\nTitle ID to view (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    const Book *book = catalogue_find_const(library, id);
    if (book == NULL) puts("Title ID not found.");
    else catalogue_print_record(library, book);
    input_pause();
}

void catalogue_add(Library *library) {
    if (library->book_count == MAX_BOOKS || library->next_book_id == INT_MAX) {
        puts("The catalogue has reached its capacity.");
        input_pause();
        return;
    }
    Book book = {0};
    book.id = library->next_book_id;
    date_today(book.added);
    puts("\nCATALOGUE A TITLE");
    input_required("Title: ", book.title, sizeof book.title);
    input_required("Author: ", book.author, sizeof book.author);
    input_text("Publisher (optional): ", book.publisher, sizeof book.publisher);
    input_text("Publication year (optional): ", book.year, sizeof book.year);
    input_text("ISBN (optional): ", book.isbn, sizeof book.isbn);
    input_text("Description (optional): ", book.description, sizeof book.description);
    library->books[library->book_count++] = book;
    library->next_book_id++;
    if (!storage_save_books(library)) {
        library->book_count--;
        library->next_book_id--;
        input_pause();
        return;
    }
    printf("Added \"%s\" to the catalogue (title ID %d).\n", book.title, book.id);
    input_pause();
}

void catalogue_withdraw(Library *library) {
    int id = input_choice("\nTitle ID to withdraw from circulation (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = catalogue_find(library, id);
    if (book == NULL || book->withdrawn) {
        puts("That active title ID was not found.");
        input_pause();
        return;
    }
    if (has_active_loan(library, id)) {
        puts("This title is currently on loan. Check it in before withdrawing it.");
        input_pause();
        return;
    }
    book->withdrawn = true;
    if (!storage_save_books(library)) book->withdrawn = false;
    else printf("Title %d withdrawn from circulation. Its record and history are retained.\n", id);
    input_pause();
}

void catalogue_remove(Library *library) {
    int id = input_choice("\nTitle ID to permanently remove (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = catalogue_find(library, id);
    if (book == NULL) {
        puts("That title ID was not found.");
        input_pause();
        return;
    }
    if (has_active_loan(library, id)) {
        puts("This title is currently on loan. Check it in before removing it.");
        input_pause();
        return;
    }
    for (size_t i = 0; i < library->loan_count; i++) {
        if (library->loans[i].book_id == id) {
            puts("This title has circulation history and cannot be permanently removed.");
            puts("Withdraw it from circulation instead to retain its record and history.");
            input_pause();
            return;
        }
    }
    char confirmation[16];
    printf("Permanently remove \"%s\" (ID %d)? This cannot be undone. (y/N): ", book->title, id);
    input_text("", confirmation, sizeof confirmation);
    if (!text_equal_case_insensitive(confirmation, "y") &&
        !text_equal_case_insensitive(confirmation, "yes")) {
        puts("Removal cancelled.");
        input_pause();
        return;
    }
    size_t index = (size_t)(book - library->books);
    Book removed = library->books[index];
    for (size_t i = index; i + 1 < library->book_count; i++) {
        library->books[i] = library->books[i + 1];
    }
    library->book_count--;
    if (!storage_save_books(library)) {
        for (size_t i = library->book_count; i > index; i--) {
            library->books[i] = library->books[i - 1];
        }
        library->books[index] = removed;
        library->book_count++;
    } else {
        printf("Title %d was permanently removed.\n", id);
    }
    input_pause();
}
