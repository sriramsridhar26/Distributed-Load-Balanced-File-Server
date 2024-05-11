# Distributed Load-Balanced File Server

## Project Description
This project implements a distributed file server system that allows multiple clients to request files concurrently. The system uses a load balancing mechanism to alternate requests between the main server(server.c) and two mirror servers(mirror1.c and mirror2.c), ensuring efficient handling of client requests under high load.

## Features
- **Multi-Client Support:** The server can handle multiple client connections concurrently.
- **Custom Command Interface:** Clients can interact with the server using a set of custom commands to request specific files, directories, and file metadata.
- **Load Balancing:** Requests are alternated between the main server and two mirror servers to balance the load.

## Commands
- `dirlist -a`: Lists all subdirectories under the server's home directory in alphabetical order.
- `dirlist -t`: Lists all subdirectories under the server's home directory in the order they were created.
- `w24fn filename`: Returns the filename, size, creation date, and permissions of the specified file.
- `w24fz size1 size2`: Returns all files whose size is between size1 and size2.
- `w24ft <extension list>`: Returns all files of the specified file types.
- `w24fdb date`: Returns all files created on or before the specified date.
- `w24fda date`: Returns all files created on or after the specified date.
- `quitc`: Terminates the client process.
