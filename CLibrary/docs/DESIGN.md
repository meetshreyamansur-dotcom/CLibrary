# Library Management System — Design

## Purpose

The application provides a local, interactive library catalogue and circulation tracker. It is implemented as a modular C11 command-line program with no external runtime dependencies. The startup screen displays the CLibrary by Shreya ASCII banner. The interface repeats a live circulation summary before each menu so collection, availability, and on-loan counts are always visible.

## Package layout and responsibilities

| Path | Responsibility |
| --- | --- |
| `include/library.h` | Shared domain records, application state, and module interfaces |
| `src/main.c` | Process entry point and application lifetime |
| `src/ui.c` | Startup banner, circulation summary, menu, and dispatch |
| `src/input.c` | Console input, validation, date, and comparison helpers |
| `src/storage.c` | Loading, validation, encoding, and saving local data |
| `src/catalogue.c` | Bibliographic records, catalogue display, withdrawal, and removal |
| `src/circulation.c` | Patron checkout and check-in workflows |
| `src/search.c` | Exact and fuzzy searches across records and circulation history |
| `src/seed.c` | Validated, duplicate-aware import of the sample title dataset |
| `seed/books.tsv` | Versioned sample bibliographic records (50 or more titles) |
| `Store/books.tsv` | Local catalogue data |
| `Store/loans.tsv` | Local checkout and check-in history |
| `build/` | Generated executable; excluded from version control |

`Makefile` compiles the source modules with C11 warnings enabled. Module interfaces and shared data structures are declared in `include/library.h`; feature implementations remain in their focused source files.

The `Store/` directory is created automatically in the process working directory if it does not already exist. Run the executable from the project directory to keep the data in the project's `Store/` folder.

## Data model

### Book

Each title has an automatically assigned integer ID, withdrawal flag, date added, title, author, publisher, publication year, ISBN, and description. Title and author are required when adding a record; remaining bibliographic metadata is optional. The withdrawal flag occupies the existing archive-flag column in the data file for format compatibility.

### Loan

Each checkout is a distinct loan record with its own ID, associated book ID, patron, checkout date, due date, optional check-in date, and optional notes. A loan with no check-in date is active. Retaining checked-in loan records preserves the complete circulation history.

### Inventory definitions

- **In collection:** non-withdrawn titles.
- **Available:** titles in the collection without an active loan.
- **On loan:** titles in the collection with an active loan.
- **Withdrawn:** titles removed from circulation but retained in the catalogue. Withdrawal is allowed only after an active loan has been checked in. Withdrawn records retain the title and its full circulation history.

Titles without circulation history can be permanently removed after confirmation. Records with circulation history cannot be permanently removed; withdraw them instead. This keeps loan references valid and preserves circulation records.

## Sample catalogue

`seed/books.tsv` is a versioned, tab-separated dataset with six bibliographic columns: title, author, publisher/edition, publication year, ISBN, and description. It contains at least 50 public-domain sample titles. The menu import is explicit: it validates the complete seed file before changes, assigns local IDs and today's date, skips records whose title and author already exist (case-insensitively), and saves the additions without altering existing records or circulation history. Sample data remains separate from local operational data in `Store/`.

## Search and details

Search compares the query, case-insensitively, with each bibliographic field and associated loan fields (patron, checkout date, due date, check-in date, and notes). Exact mode requires a whole-field match; fuzzy mode checks for a substring. Search results include withdrawn titles if their fields match. A detailed record shows circulation status, current patron and loan dates when applicable, overdue status, elapsed days on loan, and all previous loans.

## Persistence and validation

The tab-separated files use one record per line. Text fields escape percent signs, tabs, and line breaks as `%HH`, allowing descriptions and notes without corrupting record boundaries. Book records have nine fields; loan records have seven. IDs remain stable when titles are withdrawn or loans checked in. Existing data filenames and field order remain unchanged; patron and check-in terminology is represented by the corresponding existing loan columns.

The application loads both files before entering the menu and checks for malformed records and loan references to missing books. On a data error it reports the file and record context where available, then exits without attempting to rewrite the data. Operations validate input and persist the changed record set before reporting success. Each data file is replaced through a temporary file and rename; if a save fails, the in-memory operation is rolled back and an error is reported.

The program stores up to 10,000 books and 50,000 loans in memory. The flat-file implementation is intended for a single user and does not provide concurrent-writer locking or crash-atomic transactions.

## Build

```sh
make
./build/library
```
