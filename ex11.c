/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define FILES_EQUAL     (1)
#define FILES_SIMILAR   (2)
#define FILES_DIFFERENT (3)

/* Function Definitions */
int compare_files(int fd1, int fd2);
int is_whitespace(char c);
char readchar(int fd);

int main(int argc, char **argv) {
    int res;
    int f1, f2;

    if (argc < 2) {
        perror("Not enough arguments!");
        return -1;
    }

    f1 = open(argv[1], O_RDONLY, 0777);
    if (f1 == -1) {
        perror("Could not open first file");
        return -1;
    }

    f2 = open(argv[2], O_RDONLY, 0777);
    if (f2 == -1) {
        perror("Could not open the second file!");
        if (-1 == close(f1)) {
            perror("Could not close first file.");
        }
        return -1;
    }

    res = compare_files(f1, f2);
    // close the files
    if (close(f1) < 0) {
        perror("Could not close the first file.");
    }

    if (close(f2) < 0) {
        perror("could not close the second file");
    }

    return res;
}


/**
 * function receives two open file descriptors, and determines if the files are
 * equal, similar or different.
 * @param fd1 - file descriptor for the first file.
 * @param fd2 - file descriptor for the second file.
 * @return FILES_EQUAL if files are equal, FILES_SIMILAR if files are similar,
 *          FILES_DIFFERENT if files are different or if there was an error reading
 *          one of the files.
 */
int compare_files(int fd1, int fd2) {
    int equal = 1;
    char a, b;

    // read 1 byte from the file
    a = readchar(fd1);
    b = readchar(fd2);

    while (a != EOF && b != EOF) {
        if (a == b) {
            a = readchar(fd1);
            b = readchar(fd2);
            continue;
        } else {
            equal = 0;
            if(tolower(a) == tolower(b)){
                a = readchar(fd1);
                b = readchar(fd2);
                continue;
            }
            if (is_whitespace(a)) {
                a = readchar(fd1);
                continue;
            }
            if (is_whitespace(b)) {
                b = readchar(fd2);
                continue;
            }

            return FILES_DIFFERENT;
        }
    }
    // one of the files reached to end.
    // or both of them.
    if (a == EOF && b == EOF) {
        return (equal) ? FILES_EQUAL : FILES_SIMILAR;
    }
    if (a == EOF) {
        // b != EOF
        while ((b = readchar(fd2)) != EOF) {
            if (!is_whitespace(b)) {
                return FILES_DIFFERENT;
            }
        }
        return FILES_SIMILAR;
    } else {
        // a != EOF
        while ((a = readchar(fd1)) != EOF) {
            if (!is_whitespace(a)) {
                return FILES_DIFFERENT;
            }
        }
        return FILES_SIMILAR;
    }
}

/**
 * returns if the given char is a whitespace that should be ignored when comparing similar files
 * @param c - the given char
 * @return bool signifies if the given char is whitespace or not
 */
int is_whitespace(char c) {
    return (c == ' ' || c == '\n' || c == '\t');
}

/**
 * returns char from the given file descriptor
 * @param fd
 * @return returns the read char on success, EOF on end of file or error reading
 */
char readchar(int fd) {
    char c;
    ssize_t len;
    len = read(fd, &c, sizeof(char));
    if (len <= 0)
        return EOF;
    return c;
}