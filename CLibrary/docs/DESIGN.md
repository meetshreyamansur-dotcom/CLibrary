# Library Management System — Design

## Purpose

The application provides a local, interactive library catalogue and circulation tracker. It is implemented as a single C11 command-line program with no external runtime dependencies. The interface repeats a live inventory summary before each menu so availability and circulation counts are always visible.

## Components

- `main.c` contains input validation, date handling, catalogue and lending operations, search, and persistence.
- `Makefile` builds the executable with standard compiler warnings enabled.
- `Store/books.tsv` persists catalogue records.
- `Store/loans.tsv` persists checkout and return records.

The `Store/` directory is created automatically in the process working directory if it does not already exist. Run the executable from the project directory to keep the data in the project's `Store/` folder.

## Data model

### Book

Each book has an automatically assigned integer ID, archived flag, date added, title, author, publisher, publication year, ISBN, and description. Title and author are required when adding a record; remaining metadata is optional.

### Loan

Each checkout is a distinct record with its own ID, associated book ID, borrower/receiver, checkout date, due date, optional return date, and optional notes. A loan with no return date is active. Retaining returned loan records preserves the complete lending history.

### Inventory definitions

- **Catalogue:** non-archived books.
- **Available:** non-archived books without an active loan.
- **Disbursed:** non-archived books with an active loan.
- **Archived:** books excluded from active catalogue and availability counts. Archiving is allowed only after any active loan has been returned. Archival retains the book and its full lending history.

Books without lending history can be permanently removed after confirmation. Records with a lending history cannot be permanently removed; archive them instead. This keeps loan references valid and preserves circulation records.

## Search and details

Search compares the query, case-insensitively, with each book metadata field and associated loan fields (borrower, checkout date, due date, return date, and notes). Exact mode requires a whole-field match; fuzzy mode checks for a substring. Search results include archived books if their fields match. A detailed record shows current status, current borrower and loan dates when applicable, overdue status, elapsed days since checkout, and all historical loans.

## Persistence and validation

The tab-separated files use one record per line. Text fields escape percent signs, tabs, and line breaks as `%HH`, allowing descriptions and notes without corrupting record boundaries. Book records have nine fields; loan records have seven. IDs remain stable when records are archived or loans returned.

The application loads both files before entering the menu and checks for malformed records and loan references to missing books. On a data error it reports the file and record context where available, then exits without attempting to rewrite the data. Operations validate input and persist the changed record set before reporting success. Each data file is replaced through a temporary file and rename; if a save fails, the in-memory operation is rolled back and an error is reported.

The program stores up to 10,000 books and 50,000 loans in memory. The flat-file implementation is intended for a single user and does not provide concurrent-writer locking or crash-atomic transactions.

## Build

```sh
make
./library
```
