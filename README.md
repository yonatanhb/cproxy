# cproxy.c

## Overview

`cproxy.c` is a simple C program that acts as a basic HTTP proxy client. It allows users to retrieve files from remote servers via HTTP and optionally display them in a web browser.

## Features

- **HTTP File Retrieval**: The program can fetch files from remote HTTP servers.
- **Local File Handling**: It checks if the requested file exists locally and serves it if available.
- **Browser Integration**: With the `-s` flag, it can open fetched files directly in the default web browser.
- **Error Handling**: Provides meaningful error messages for various failure scenarios.

## Dependencies

- Standard C libraries: `stdio.h`, `stdlib.h`, `string.h`, `sys/socket.h`, `netinet/in.h`, `netdb.h`, `unistd.h`, `fcntl.h`, `sys/stat.h`.

## Usage

To compile the program, use any standard C compiler:

```bash
gcc cproxy.c -o cproxy
```

To run the program, use the following command:

```bash
./cproxy <URL> [-s]
```

- `<URL>`: The URL of the file to fetch via HTTP.
- `[-s]`: Optional flag to open the fetched file in the default web browser.

## Example

```bash
./cproxy http://example.com/index.html
```

This command fetches the `index.html` file from `http://example.com`.

```bash
./cproxy http://example.com/image.jpg -s
```

This command fetches the `image.jpg` file from `http://example.com` and opens it in the default web browser.

## Notes

- Make sure to replace `<URL>` with the actual URL of the file you want to retrieve.
- The program automatically handles local file storage and retrieval. If the requested file exists locally, it serves it from the filesystem.
- When using the `-s` flag, ensure that a default web browser is properly configured on your system.

--- 
