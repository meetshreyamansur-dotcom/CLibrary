# Library Management System

A professional, modular library management application written in C. It runs in a terminal, maintains a searchable catalogue, and tracks circulation using standard library terminology: patrons, checkouts, check-ins, due dates, and titles on loan.

## Project structure

```text
CLibrary/
├── include/
│   └── library.h          Shared domain types and module interfaces
├── src/
│   ├── main.c             Application entry point
│   ├── ui.c               Startup banner, menu, and circulation summary
│   ├── input.c            Validated input, date, and text helpers
│   ├── storage.c          Persistent catalogue and circulation records
│   ├── catalogue.c        Catalogue, title records, withdrawal, removal
│   ├── circulation.c      Patron checkout and check-in workflows
│   ├── search.c           Exact and fuzzy catalogue search
│   └── seed.c             Safe import of the bundled sample catalogue
├── seed/
│   └── books.tsv          50+ sample title records
├── Store/                 Local application data (not committed)
├── build/                 Generated executable (not committed)
├── docs/
│   └── DESIGN.md          Architecture and data-format documentation
├── Makefile
└── README.md
```

## Capabilities

- **Circulation summary:** Every menu screen shows the number of titles in the collection, titles available, titles on loan, and titles withdrawn from circulation.
- **Catalogue:** Browse the active catalogue, review withdrawn titles, and open a detailed title record by ID.
- **Search:** Search case-insensitively across titles, authors, publishers, publication years, ISBNs, descriptions, patron names, checkout/due/check-in dates, and circulation notes. Exact mode matches a complete field; fuzzy mode finds a query anywhere within a field. Results show the full title record and circulation history.
- **Bibliographic records:** Catalogue titles with required title and author, plus optional publisher, publication year, ISBN, and description. Each title receives a stable ID and an automatically recorded date added.
- **Checkout and check-in:** Check available titles out to named patrons, record due dates and optional notes, and check returned titles in. Title details show the current patron, checkout date, elapsed time on loan, due date, overdue status, and previous circulation history.
- **Withdrawal and removal:** Withdraw returned titles from circulation without deleting their catalogue records or circulation history. Titles currently on loan cannot be withdrawn. Permanently remove titles only when they have no circulation history; titles with history must be withdrawn instead. Permanent removal requires confirmation.
- **Sample catalogue:** Import at least 50 bundled public-domain title records from `seed/books.tsv` using the explicit **Load the sample catalogue** menu option. Import adds missing titles, skips matching title/author pairs, and does not replace existing catalogue records or circulation data.
- **Persistent storage:** Data is kept locally in `Store/books.tsv` and `Store/loans.tsv`; the application creates the `Store/` directory on first run. Fields are safely escaped in the tab-separated files.

## Requirements

- A C compiler supporting C11 (for example, Clang or GCC)
- `make` (optional; a direct compiler command is also provided)

## Build and run

```sh
make
./build/library
```

Or compile without `make`:

```sh
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -O2 \
  src/main.c src/ui.c src/input.c src/storage.c src/catalogue.c \
  src/circulation.c src/search.c src/seed.c -o build/library
./build/library
```

Run the program from the project directory so it can locate `seed/books.tsv` and keep application data in `Store/`. To remove the generated executable, run `make clean`; this does not remove library data.

## Data and backup

`Store/books.tsv` contains bibliographic records, including withdrawal status (stored in the existing archive-flag column). `Store/loans.tsv` stores each checkout as a separate circulation record; completed records remain available as history. The file names and tab-separated record format are retained. Both files are plain-text records with percent-escaped delimiters and line breaks. Back up the entire `Store/` directory together and do not edit the files while the application is running.

The program reports malformed or unreadable data and stops rather than silently discarding records. The current implementation supports up to 10,000 books and 50,000 lending records.

## Interaction notes

- Dates use the `YYYY-MM-DD` format and are validated.
- Book and loan IDs are assigned automatically and are not reused.
- Withdrawn titles are excluded from active collection totals and normal catalogue listings.
- This is a single-user, local application; concurrent instances should not edit the same data files.

## Design documentation

See [docs/DESIGN.md](docs/DESIGN.md) for the module responsibilities, data model, seed import, persistence format, and operational rules.
