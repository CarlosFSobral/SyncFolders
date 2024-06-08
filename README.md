# SyncFolders

Step 1- Clone the repository: 

    git clone <repository_url>

    cd <repository_directory>

Step 2- Make sure that OpenSSL is installed and configured (v3.3.1 used)

Step 3- Run the program: 

    <executable_name> <source_path> <replica_path> <interval_seconds> <log_file_path>
For example: 

      FolderSync.exe "C:\Source" "C:\Replica" 60 "C:\Logs\sync.log"

Arguments: 

<source_path>: Path to the source directory to be synchronized.

<replica_path>: Path to the replica directory where the source will be mirrored.

<interval_seconds>: Interval in seconds at which the synchronization will occur.

<log_file_path>: Path to the log file where synchronization operations will be recorded.

Usage Example: 

      .\SyncFolders.exe C:\Users\Source C:\Users\Replica 60 C:\Users\sync.log

Logging:
Logs are written to the specified log file with timestamps. Example log entries:

      [2024-06-08 12:00:00] Starting folder synchronization.
      [2024-06-08 12:00:00] Source path: C:\Users\Example\Source
      [2024-06-08 12:00:00] Replica path: D:\Backup\Replica
      [2024-06-08 12:00:00] Synchronization interval: 60 seconds
      [2024-06-08 12:00:01] Copied file: C:\Users\Example\Source\file.txt to D:\Backup\Replica\file.txt
      [2024-06-08 12:01:00] Removed: D:\Backup\Replica\oldfile.txt
      [2024-06-08 12:01:00] Synchronization complete. All files and directories are synchronized.
      [2024-06-08 12:02:00] Synchronization stopped.

Stopping the Program:
To stop the program, send a SIGINT (Ctrl+C) or SIGTERM signal. The program will log the termination and stop gracefully:

      [2024-06-08 12:02:00] Synchronization stopped.

Notes:

Ensure the source directory is accessible and exists before starting the synchronization.

The program will create the replica directory if it does not exist.
