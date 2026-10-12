# Operating Systems Programming Assignment 02

## Feature 2: Complete Long Listing Format

### 1. What is the crucial difference between `stat()` and `lstat()`? In the context of the `ls` command, when is it more appropriate to use `lstat()`?

Both `stat()` and `lstat()` retrieve metadata about a filesystem path, such as
the file type, permissions, owner, group, size, and modification time. The
important difference is how they handle symbolic links.

`stat()` follows a symbolic link and returns metadata about the file or
directory to which the link points. For example, if `shortcut` points to
`original.txt`, `stat("shortcut", ...)` reports the metadata of
`original.txt`.

`lstat()` does not follow the symbolic link. It returns metadata about the
link itself, so the file type can be identified as a symbolic link using
`S_ISLNK(st_mode)`.

`lstat()` is more appropriate for an `ls` implementation because `ls` should
be able to identify symbolic links and display them as links. It also allows
the program to print the link target separately, for example:

```text
shortcut -> original.txt
```

If `stat()` were used instead, a symbolic link could be treated as the type
of its target and the program could lose information about the link itself.

### 2. The `st_mode` field in `struct stat` is an integer that contains both the file type and the permission bits. Explain how bitwise operators and predefined macros extract this information.

The `st_mode` field is a bit field. Different bits represent the file type
and the read, write, and execute permissions.

The predefined `S_IS...` macros test the file type:

```c
S_ISREG(mode)  /* regular file */
S_ISDIR(mode)  /* directory */
S_ISLNK(mode)  /* symbolic link */
S_ISCHR(mode)  /* character device */
S_ISBLK(mode)  /* block device */
S_ISFIFO(mode) /* named pipe */
S_ISSOCK(mode) /* socket */
```

Individual permission bits are tested with the bitwise AND operator (`&`).
For example:

```c
if (mode & S_IRUSR)
    putchar('r');
else
    putchar('-');
```

This checks whether the owner-read bit is set. The permission macros are
grouped by owner, group, and others:

```c
S_IRUSR  S_IWUSR  S_IXUSR  /* owner read, write, execute */
S_IRGRP  S_IWGRP  S_IXGRP  /* group read, write, execute */
S_IROTH  S_IWOTH  S_IXOTH  /* others read, write, execute */
```

The expression `mode & S_IRUSR` produces a nonzero value when the
owner-read bit is present. A zero result means that permission is absent.
The same method is used for all other permission bits.

Special permission bits can also be checked:

```c
mode & S_ISUID /* set-user-ID */
mode & S_ISGID /* set-group-ID */
mode & S_ISVTX /* sticky bit */
```

The Feature 2 program uses this information in `mode_to_string()` to build
the familiar ten-character permission string. The first character represents
the file type, and the remaining nine characters represent owner, group, and
other permissions:

```text
-rwxr-xr--
```

For example, `-` means a regular file, `rwx` gives the owner read/write/
execute permission, `r-x` gives the group read/execute permission, and `r--`
gives other users read-only permission.

## Feature 2 Implementation Summary

The `-l` option uses `getopt()` to select long-listing mode. For every visible
directory entry, the program uses `lstat()` to collect metadata, resolves the
numeric user and group IDs with `getpwuid()` and `getgrgid()`, formats the
modification time with `ctime()`, and prints aligned columns.

The feature was built and tested with:

```bash
make clean
make
./bin/ls -l
```

## Feature 3: Column Display (Down Then Across)

### 1. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop insufficient?

The program first reads all visible names into a dynamically allocated array.
While reading, it records the longest name. It then calculates the number of
columns that fit in the available terminal width:

```text
columns = terminal_width / (longest_name + spacing)
rows = (number_of_items + columns - 1) / columns
```

The output is printed row by row, but the array is indexed column by column:

```text
index = row + column * rows
```

This produces the required "down then across" layout. A simple loop through
the array would print entries in one sequential order and would not know
which later item belongs in the next column of the current row. The complete
array and row/column calculation are therefore needed before printing.

### 2. What is the purpose of `ioctl` here? What are the limitations of using a fixed 80-column fallback?

`ioctl()` is a system call that can query terminal properties. With the
`TIOCGWINSZ` request, the program obtains the current terminal width from a
`struct winsize`. The column count can then adapt when the terminal is
resized.

The program uses 80 columns if terminal-size detection fails, such as when
output is redirected or no terminal is attached. A fixed fallback is less
accurate on terminals narrower or wider than 80 columns. It can cause
unnecessary wrapping, excessive whitespace, or lines that extend past the
actual display width. It is useful as a safe fallback, but dynamic detection
is preferable.

### Feature 3 implementation summary

The default `do_ls()` behavior now collects names, determines the terminal
width with `ioctl(TIOCGWINSZ)`, and prints names down each column before
moving across. The `-l` path remains unchanged and continues to print the
long listing format.