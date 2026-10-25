#ifndef CLIBRARY_STORAGE_H
#define CLIBRARY_STORAGE_H

#include "library.h"

bool storage_load(Library *library);
bool storage_save_books(const Library *library);
bool storage_save_loans(const Library *library);

#endif