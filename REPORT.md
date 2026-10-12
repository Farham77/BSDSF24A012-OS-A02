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

## Feature 4: Horizontal Column Display (`-x`)

### 1. Compare the "down then across" and "across" printing logic. Which requires more pre-calculation?

The default down-then-across mode calculates both the number of columns and
the number of rows. It uses the index:

```text
row + column * rows
```

This rearranges the sequential array into column-major output, so it requires
more pre-calculation and indexing logic.

The `-x` mode is row-major. It loops through the array in its existing order,
tracks the current horizontal position, and starts a new line when the next
column would exceed the terminal width. It still needs the longest filename
and terminal width to determine column spacing, but it does not need a row
calculation or column-major index formula. Therefore, the default
down-then-across mode is more complex.

### 2. How does the program manage `-l`, `-x`, and default display modes?

`getopt()` recognizes both `l` and `x`. The program stores the selected modes
in `long_format` and `horizontal` flags:

```c
while ((option = getopt(argc, (char * const *)argv, "lx")) != -1)
```

If `-l` is present, `main()` calls `long_ls()`. Otherwise it calls `do_ls()`
and passes the horizontal flag. `do_ls()` calls the shared `print_names()`
helper, which selects row-major output for `-x` or the existing
down-then-across output when no display option is supplied.

## Feature 4 implementation summary

The new `-x` mode prints names left to right and wraps when the next padded
column would exceed the detected terminal width. The default mode and `-l`
mode remain available.

## Feature 5: Alphabetical Sort

### 1. Why must entries be read into memory before sorting? What are the drawbacks for directories with millions of files?

Sorting requires access to the complete collection of entries. The program
therefore reads the visible names into a dynamic array before calling
`qsort()`. If entries were printed immediately while reading the directory,
later entries could not be moved before earlier output.

The main drawback is memory usage. A directory containing millions of files
requires memory for the array, each copied filename, and, in long-listing
mode, each stored `struct stat`. Reading everything also delays the first
output and can make allocation or sorting expensive. The approach is simple
and suitable for this assignment, but a production utility must account for
very large directories.

### 2. Explain the comparison function required by `qsort()`. Why does it use `const void *` arguments?

`qsort()` is a generic C library sorting function, so it does not know the
element type. Its comparison function receives pointers to two array
elements as `const void *`:

```c
static int compare_names(const void *left, const void *right)
{
    const char *const *first = left;
    const char *const *second = right;
    return strcmp(*first, *second);
}
```

The arguments are `void *` so the same `qsort()` function can sort many data
types. They are `const` because the comparison must not modify the elements.
The comparator casts them to the actual element type and returns a negative
value, zero, or a positive value when the first element is respectively
less than, equal to, or greater than the second.

Feature 5 uses one comparator for the `char **` name array and another for
the `FileInfo` array used by `-l`. This ensures alphabetical ordering applies
to default, `-x`, and long-listing output.

## Feature 6: Colorized Output

### 1. How do ANSI escape codes produce color in a standard Linux terminal? Show the code sequence for green text.

ANSI escape codes are special characters interpreted by terminal emulators as
formatting instructions. The escape character is followed by a control
sequence. For example, green text can be printed and then reset with:

```c
printf("\033[0;32mgreen text\033[0m");
```

`\033[0;32m` selects green, and `\033[0m` resets the terminal style. The
program defines color constants and prints the reset code after every name so
later output does not accidentally inherit the color.

### 2. Which permission bits determine whether a file is executable?

The execute bits in `st_mode` are:

```c
S_IXUSR /* owner execute */
S_IXGRP /* group execute */
S_IXOTH /* others execute */
```

The program checks them together:

```c
mode & (S_IXUSR | S_IXGRP | S_IXOTH)
```

A nonzero result means at least one class of user has execute permission.
Executable files are printed green.

### Feature 6 implementation summary

The program uses `lstat()` to determine each entry's type and selects colors
with `color_for()`: directories are blue, executables green, `.tar`, `.gz`,
and `.zip` archives red, symbolic links pink, and special files reverse
video. The same color logic is used by column output and long listing.

## Feature 7: Recursive Listing (`-R`)

### 1. What is a base case? What stops recursion in this implementation?

A base case is the condition under which a recursive function stops making
recursive calls. Without a base case, recursion would continue until the
process ran out of stack or another resource.

In this program, recursion stops naturally when a directory has no visible
subdirectories. The function also recurses only when `lstat()` succeeds and
`S_ISDIR(st_mode)` is true. It does not recurse into regular files or
symbolic links. Entries beginning with `.` are skipped, so `.` and `..` do
not create cycles.

### 2. Why construct the full path before recursion? What happens if only the child name is used?

The full path preserves the relationship between the current directory and
the child:

```text
parent_dir/subdir
```

The function uses `snprintf()` to construct this path and passes it to the
recursive call. If it called `do_ls("subdir")` or the recursive function with
only `"subdir"`, the path would be interpreted relative to the process's
current working directory, not necessarily relative to `parent_dir`. The
program could fail to open the directory or open an unrelated directory with
the same name.

### Feature 7 implementation summary

`-R` selects `recursive_ls()`. It prints a directory header, uses the
existing sorted/colorized/default or long-listing logic, collects child names,
sorts them, constructs a full path for each child, and recursively visits
directories. `lstat()` prevents symbolic links to directories from causing
unexpected traversal.