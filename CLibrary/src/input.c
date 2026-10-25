#define _POSIX_C_SOURCE 200809L

#include "library.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void input_text(const char *prompt, char *out, size_t capacity) {
    char line[LINE_SIZE];
    for (;;) {
        printf("%s", prompt);
        if (fgets(line, sizeof line, stdin) == NULL) {
            puts("\nEnd of input. Goodbye.");
            exit(EXIT_SUCCESS);
        }
        size_t length = strcspn(line, "\r\n");
        if (line[length] == '\0' && length == sizeof line - 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            puts("Input is too long. Please try again.");
            continue;
        }
        line[length] = '\0';
        while (length > 0 && isspace((unsigned char)line[length - 1])) {
            line[--length] = '\0';
        }
        char *start = line;
        while (isspace((unsigned char)*start)) start++;
        if (strlen(start) >= capacity) {
            printf("Please enter no more than %zu characters.\n", capacity - 1);
            continue;
        }
        memmove(out, start, strlen(start) + 1);
        return;
    }
}

void input_required(const char *prompt, char *out, size_t capacity) {
    do {
        input_text(prompt, out, capacity);
        if (out[0] == '\0') puts("This field is required.");
    } while (out[0] == '\0');
}

int input_choice(const char *prompt, int minimum, int maximum) {
    char text[32];
    char *end;
    for (;;) {
        input_text(prompt, text, sizeof text);
        errno = 0;
        long value = strtol(text, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (text[0] != '\0' && *end == '\0' && errno == 0 &&
            value >= minimum && value <= maximum) return (int)value;
        printf("Enter a number from %d to %d.\n", minimum, maximum);
    }
}

bool date_valid(const char *text) {
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    for (size_t i = 0; i < 10; i++) {
        if (i != 4 && i != 7 && !isdigit((unsigned char)text[i])) return false;
    }
    int year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 +
               (text[2] - '0') * 10 + text[3] - '0';
    int month = (text[5] - '0') * 10 + text[6] - '0';
    int day = (text[8] - '0') * 10 + text[9] - '0';
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1 || month < 1 || month > 12) return false;
    int maximum = days[month - 1];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) maximum++;
    return day >= 1 && day <= maximum;
}

long date_ordinal(const char *date) {
    int year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
               (date[2] - '0') * 10 + date[3] - '0';
    int month = (date[5] - '0') * 10 + date[6] - '0';
    int day = (date[8] - '0') * 10 + date[9] - '0';
    year -= month <= 2;
    long era = (year >= 0 ? year : year - 399) / 400;
    unsigned year_of_era = (unsigned)(year - era * 400);
    unsigned day_of_year =
        (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U +
        (unsigned)day - 1U;
    unsigned day_of_era = year_of_era * 365U + year_of_era / 4U -
                          year_of_era / 100U + day_of_year;
    return era * 146097L + (long)day_of_era;
}

void date_today(char out[11]) {
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local == NULL || strftime(out, 11, "%Y-%m-%d", local) == 0) {
        fputs("Unable to determine today's date.\n", stderr);
        exit(EXIT_FAILURE);
    }
}

void input_date(const char *prompt, char out[11]) {
    for (;;) {
        input_text(prompt, out, 11);
        if (date_valid(out)) return;
        puts("Use a valid date in YYYY-MM-DD format.");
    }
}

void input_pause(void) {
    char line[8];
    printf("\nPress Enter to continue...");
    (void)fgets(line, sizeof line, stdin);
}

bool text_equal_case_insensitive(const char *left, const char *right) {
    while (*left && *right) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) return false;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

bool text_contains_case_insensitive(const char *text, const char *query) {
    if (*query == '\0') return true;
    for (; *text; text++) {
        const char *candidate = text;
        const char *needle = query;
        while (*candidate && *needle &&
               tolower((unsigned char)*candidate) == tolower((unsigned char)*needle)) {
            candidate++;
            needle++;
        }
        if (*needle == '\0') return true;
    }
    return false;
}
