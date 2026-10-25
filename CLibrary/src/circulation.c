#include "library.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

Loan *circulation_active_loan(Library *library, int book_id) {
    for (size_t i = 0; i < library->loan_count; i++) {
        Loan *loan = &library->loans[i];
        if (loan->book_id == book_id && loan->checked_in[0] == '\0') return loan;
    }
    return NULL;
}

void circulation_checkout(Library *library) {
    if (library->loan_count == MAX_LOANS || library->next_loan_id == INT_MAX) {
        puts("The circulation history has reached its capacity.");
        input_pause();
        return;
    }
    int id = input_choice("\nTitle ID to check out (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Book *book = catalogue_find(library, id);
    if (book == NULL || !catalogue_available(library, book)) {
        puts("That title is unavailable. Check its ID and circulation status.");
        input_pause();
        return;
    }
    Loan loan = {0};
    loan.id = library->next_loan_id;
    loan.book_id = id;
    date_today(loan.checkout);
    input_required("Patron name: ", loan.patron, sizeof loan.patron);
    input_date("Due date (YYYY-MM-DD): ", loan.due);
    if (date_ordinal(loan.due) < date_ordinal(loan.checkout)) {
        puts("Due date cannot be earlier than today.");
        input_pause();
        return;
    }
    input_text("Loan notes (optional): ", loan.notes, sizeof loan.notes);
    library->loans[library->loan_count++] = loan;
    library->next_loan_id++;
    if (!storage_save_loans(library)) {
        library->loan_count--;
        library->next_loan_id--;
        input_pause();
        return;
    }
    printf("\"%s\" has been checked out to %s.\n", book->title, loan.patron);
    input_pause();
}

void circulation_checkin(Library *library) {
    int id = input_choice("\nTitle ID being checked in (0 cancels): ", 0, INT_MAX);
    if (id == 0) return;
    Loan *loan = circulation_active_loan(library, id);
    if (loan == NULL) {
        puts("No active loan was found for that title ID.");
        input_pause();
        return;
    }
    char checked_in[11];
    date_today(checked_in);
    strcpy(loan->checked_in, checked_in);
    if (!storage_save_loans(library)) loan->checked_in[0] = '\0';
    else printf("Check-in recorded for title %d from patron %s.\n", id, loan->patron);
    input_pause();
}
