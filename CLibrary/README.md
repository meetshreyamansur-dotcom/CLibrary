# Library Management System

A professional, menu-driven library management application written in C. It runs in a terminal, maintains a searchable book catalogue, and records current and historical loans.

## Capabilities

- **Live library overview:** Every menu screen shows the number of active catalogue books, books available to borrow, books currently disbursed, and archived books.
- **Catalogue:** List active books, review archived books, and open a detailed record by book ID.
- **Search:** Search case-insensitively across book titles, authors, publishers, publication years, ISBNs, descriptions, borrower names, checkout/due/return dates, and loan notes. Exact mode matches a complete field; fuzzy mode finds a query anywhere within a field. A match displays the complete book record and its lending history.
- **Book records:** Add books with title and author, plus optional publisher, publication year, ISBN, and description. Each book receives a stable ID and an automatically recorded date added.
- **Lending and returns:** Lend available books to named borrowers, record due dates and optional notes, and record returns. Book details show who currently has a disbursed book, checkout date, elapsed time, due date, overdue status, and historical loans.
- **Archiving and removal:** Archive returned books without deleting their catalogue record or loan history. Disbursed books cannot be archived. Permanently remove books with no lending history; books with history must be archived instead so circulation records remain intact. Permanent removal requires confirmation.
- **Persistent storage:** Data is kept locally in `Store/books.tsv` and `Store/loans.tsv`; the application creates the `Store/` directory on first run. Fields are safely escaped in the tab-separated files.

## Requirements

- A C compiler supporting C11 (for example, Clang or GCC)
- `make` (optional; a direct compiler command is also provided)

## Build and run

```sh
make
./library
```

Or compile without `make`:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -O2 -o library main.c
./library
```

Run the program from the project directory so that the `Store/` data directory remains there. To remove only the compiled executable, run `make clean`; this does not remove library data.

## Data and backup

`Store/books.tsv` contains the catalogue, including archived status. `Store/loans.tsv` stores each checkout as a separate record; completed records remain available as lending history. Both files are plain-text, tab-separated records with percent-escaped delimiters and line breaks. Back up the entire `Store/` directory together and do not edit the files while the application is running.

The program reports malformed or unreadable data and stops rather than silently discarding records. The current implementation supports up to 10,000 books and 50,000 lending records.

## Interaction notes

- Dates use the `YYYY-MM-DD` format and are validated.
- Book and loan IDs are assigned automatically and are not reused.
- Archived records are excluded from active catalogue totals and normal catalogue listings.
- This is a single-user, local application; concurrent instances should not edit the same data files.

## Design documentation

See [docs/DESIGN.md](docs/DESIGN.md) for the design, data model, persistence format, and operational rules.
