#include "library.h"

#include <stdio.h>

void ui_banner(void) {
    puts("+----------------------------------------------------------+");
    puts("|                                                          |");
    puts("|   C L I B R A R Y                                        |");
    puts("|   by Shreya                                              |");
    puts("|                                                          |");
    puts("|   LIBRARY CATALOGUE  |  CIRCULATION MANAGEMENT           |");
    puts("|                                                          |");
    puts("+----------------------------------------------------------+");
}

void ui_dashboard(const Library *library) {
    size_t in_collection = 0, available = 0, on_loan = 0, withdrawn = 0;
    for (size_t i = 0; i < library->book_count; i++) {
        const Book *book = &library->books[i];
        if (book->withdrawn) {
            withdrawn++;
            continue;
        }
        in_collection++;
        bool loan_active = false;
        for (size_t j = 0; j < library->loan_count; j++) {
            const Loan *loan = &library->loans[j];
            if (loan->book_id == book->id && loan->checked_in[0] == '\0') {
                loan_active = true;
                break;
            }
        }
        if (loan_active) on_loan++;
        else available++;
    }
    printf("\n"
           "  CIRCULATION SUMMARY  |  In collection: %zu  |  Available: %zu  |  On loan: %zu  |  Withdrawn: %zu\n"
           "  -------------------------------------------------------------------------------------------------\n",
           in_collection, available, on_loan, withdrawn);
}

void ui_menu(void) {
    puts("  CATALOGUE & CIRCULATION");
    puts("  1. Browse the catalogue");
    puts("  2. Search the catalogue (exact or fuzzy)");
    puts("  3. View a title record and circulation history");
    puts("  4. Catalogue a new title");
    puts("  5. Check out a title to a patron");
    puts("  6. Check in a returned title");
    puts("  7. Withdraw a title from circulation");
    puts("  8. Permanently remove a title (no circulation history)");
    puts("  9. View withdrawn titles");
    puts(" 10. Load the sample catalogue (50+ titles)");
    puts("  0. Exit");
}

bool ui_startup(Library *library) {
    ui_banner();
    if (!storage_load(library)) {
        fprintf(stderr, "Library data could not be loaded. No changes have been made.\n");
        return false;
    }
    return true;
}

void ui_run(Library *library) {
    for (;;) {
        ui_dashboard(library);
        ui_menu();
        int choice = input_choice("\nSelect an option: ", 0, 10);
        switch (choice) {
            case 1: catalogue_list(library, false); input_pause(); break;
            case 2: search_catalogue(library); break;
            case 3: catalogue_details(library); break;
            case 4: catalogue_add(library); break;
            case 5: circulation_checkout(library); break;
            case 6: circulation_checkin(library); break;
            case 7: catalogue_withdraw(library); break;
            case 8: catalogue_remove(library); break;
            case 9: catalogue_list(library, true); input_pause(); break;
            case 10:
                (void)seed_import(library);
                input_pause();
                break;
            case 0: puts("\nLibrary data saved. Goodbye."); return;
        }
    }
}
