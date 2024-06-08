#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <exception>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <mutex>
#include <openssl/sha.h>

namespace fs = std::filesystem;

std::mutex logMutex;  ///< Mutex to protect log file operations
std::atomic<bool> keepRunning(true);  // Atomic flag to control the running state of the program
std::atomic<bool> changesMade(false);  // Atomic flag to track changes during synchronization

/**
 * brief Signal handler for SIGINT and SIGTERM to gracefully stop the synchronization
 * param signal Signal number
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        keepRunning = false;
    }
}

/**
 * brief Get current time as a string in "YYYY-MM-DD HH:MM:SS" format
 * return Current time as a string
 */
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);  // Use localtime_s for safety

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * brief Log operations to both log file and console
 * param logFilePath Path to the log file
 * param message Message to log
 */
void logOperation(const std::string& logFilePath, const std::string& message) {
    std::lock_guard<std::mutex> guard(logMutex);
    std::string logEntry = "[" + getCurrentTime() + "] " + message;
    std::ofstream logFile(logFilePath, std::ios_base::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: Unable to open log file: " << logFilePath << std::endl;
        return;
    }
    logFile << logEntry << std::endl;
    std::cout << logEntry << std::endl;
}

/**
 * brief Compute SHA-256 hash of a file
 * param path Path to the file
 * return SHA-256 hash as a string
 */
std::string computeFileHash(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)&buffer[0], buffer.size(), hash);
    std::ostringstream hashStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return hashStream.str();
}

/**
 * brief Synchronize files from source to replica
 * param source Source directory path
 * param replica Replica directory path
 * param logFilePath Path to the log file
 */
void syncCopy(const fs::path& source, const fs::path& replica, const std::string& logFilePath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(source)) {
            const auto& path = entry.path();
            auto relativePath = fs::relative(path, source);
            auto replicaPath = replica / relativePath;

            if (fs::is_regular_file(path)) {
                bool shouldCopy = false;
                if (!fs::exists(replicaPath)) {
                    shouldCopy = true;
                }
                else {
                    std::string sourceHash = computeFileHash(path);
                    std::string replicaHash = computeFileHash(replicaPath);
                    if (sourceHash != replicaHash) {
                        shouldCopy = true;
                    }
                }

                if (shouldCopy) {
                    fs::copy_file(path, replicaPath, fs::copy_options::overwrite_existing);
                    logOperation(logFilePath, "Copied file: " + path.string() + " to " + replicaPath.string());
                    changesMade = true;
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        logOperation(logFilePath, "Filesystem error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        logOperation(logFilePath, "Error: " + std::string(e.what()));
    }
}


/**
 * brief Delete files from replica that do not exist in source
 * param source Source directory path
 * param replica Replica directory path
 * param logFilePath Path to the log file
 */
void syncDelete(const fs::path& source, const fs::path& replica, const std::string& logFilePath) {
    try {
        std::vector<fs::path> filesToRemove;

        for (const auto& entry : fs::recursive_directory_iterator(replica)) {
            const auto& path = entry.path();
            auto relativePath = fs::relative(path, replica);
            auto sourcePath = source / relativePath;

            if (!fs::exists(sourcePath)) {
                filesToRemove.push_back(path);
            }
        }

        for (const auto& path : filesToRemove) {
            fs::remove_all(path);
            logOperation(logFilePath, "Removed: " + path.string());
            changesMade = true;  // Flag changes
        }
    }
    catch (const fs::filesystem_error& e) {
        logOperation(logFilePath, "Filesystem error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        logOperation(logFilePath, "Error: " + std::string(e.what()));
    }
}

/**
 * brief Synchronize subdirectories from source to replica
 * param source Source directory path
 * param replica Replica directory path
 * param logFilePath Path to the log file
 */
void syncSubdirectories(const fs::path& source, const fs::path& replica, const std::string& logFilePath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(source)) {
            const auto& path = entry.path();
            auto relativePath = fs::relative(path, source);
            auto replicaPath = replica / relativePath;

            if (fs::is_directory(path)) {
                // Create directory in replica if it does not exist
                if (!fs::exists(replicaPath)) {
                    fs::create_directory(replicaPath);
                    logOperation(logFilePath, "Created directory: " + replicaPath.string());
                    changesMade = true;  // Flag changes
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        logOperation(logFilePath, "Filesystem error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        logOperation(logFilePath, "Error: " + std::string(e.what()));
    }
}

/**
 * brief Main synchronization function that calls subdirectory, copy, and delete operations
 * param source Source directory path
 * param replica Replica directory path
 * param logFilePath Path to the log file
 */
void syncFolders(const fs::path& source, const fs::path& replica, const std::string& logFilePath) {
    changesMade = false;  // Reset changes flag at the beginning of synchronization
    try {
        // Ensure replica exists
        if (!fs::exists(replica)) {
            fs::create_directory(replica);
            logOperation(logFilePath, "Created replica directory: " + replica.string());
            changesMade = true;  // Flag changes
        }

        // Sync subdirectories
        syncSubdirectories(source, replica, logFilePath);

        // Sync copy operations
        syncCopy(source, replica, logFilePath);

        // Sync delete operations
        syncDelete(source, replica, logFilePath);
    }
    catch (const fs::filesystem_error& e) {
        logOperation(logFilePath, "Filesystem error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        logOperation(logFilePath, "Error: " + std::string(e.what()));
    }
}

/**
 * brief Validate if the source path is a valid directory
 * param source Source directory path
 * param logFilePath Path to the log file
 * return True if the source path is valid, false otherwise
 */
bool isSourceValid(const fs::path& source, const std::string& logFilePath) {
    if (!fs::exists(source)) {
        logOperation(logFilePath, "Error: Source path does not exist.");
        return false;
    }
    if (!fs::is_directory(source)) {
        logOperation(logFilePath, "Error: Source path is not a directory.");
        return false;
    }
    return true;
}

/**
 * brief Count the total number of files and directories in a given path
 * param directory Directory path
 * return Total count of files and directories
 */
int countFilesAndDirectories(const fs::path& directory) {
    int count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (fs::is_regular_file(entry) || fs::is_directory(entry)) {
            ++count;
        }
    }
    return count;
}

/**
 * brief Check if the synchronization is complete by comparing the number of files and directories
 * param source Source directory path
 * param replica Replica directory path
 * param logFilePath Path to the log file
 */
void checkSyncCompletion(const fs::path& source, const fs::path& replica, const std::string& logFilePath) {
    int sourceCount = countFilesAndDirectories(source);
    int replicaCount = countFilesAndDirectories(replica);

    if (sourceCount == replicaCount && changesMade) {
        logOperation(logFilePath, "Synchronization complete. All files and directories are synchronized.");
        changesMade = false;  // Reset changes flag after logging completion
    }
}

/**
 * brief Main function to handle input arguments and initiate synchronization process
 * param argc Argument count
 * param argv Argument values
 * return Exit status
 */
int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <source_path> <replica_path> <interval_seconds> <log_file_path>" << std::endl;
        return 1;
    }

    fs::path sourcePath = argv[1];
    fs::path replicaPath = argv[2];
    int interval = std::stoi(argv[3]);
    std::string logFilePath = argv[4];

    // If the source is invalid, return
    if (!isSourceValid(sourcePath, logFilePath)) {
        return 1;
    }

    logOperation(logFilePath, "Starting folder synchronization.");
    logOperation(logFilePath, "Source path: " + sourcePath.string());
    logOperation(logFilePath, "Replica path: " + replicaPath.string());
    logOperation(logFilePath, "Synchronization interval: " + std::to_string(interval) + " seconds");

    // Set up signal handling for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    while (keepRunning) {
        if (!isSourceValid(sourcePath, logFilePath)) {
            logOperation(logFilePath, "Source directory has been deleted or is inaccessible. Exiting...");
            return 1;
        }

        auto start = std::chrono::steady_clock::now();

        // Sync folders
        syncFolders(sourcePath, replicaPath, logFilePath);

        // Check synchronization completion
        checkSyncCompletion(sourcePath, replicaPath, logFilePath);

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        // Sleep for the remaining interval
        std::this_thread::sleep_for(std::chrono::seconds(interval) - elapsed_seconds);
    }

    logOperation(logFilePath, "Synchronization stopped.");
    return 0;
}
