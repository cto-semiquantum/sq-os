#ifndef SQPKG_H
#define SQPKG_H

/* sqpkg_execute — processes and runs package manager commands
 * subcmd : update, search, install, remove, list
 * arg    : package name or query string (optional) */
void sqpkg_execute(const char *subcmd, const char *arg);

#endif /* SQPKG_H */
