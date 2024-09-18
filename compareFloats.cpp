#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <limits>
#include <cstring>
#include <sstream>
#include <cstdint>

#pragma pack(push, 1)
struct FileHeader {
    char magic[4];  // Magic number to identify the file type
    uint32_t version;  // Version of the file format
    uint32_t dataType;  // 0 for Simple, 1 for Double, 2 for Extended
    uint32_t dataSize;  // Size of each data element in bytes
    uint32_t elementCount;  // Number of elements in the file
    uint32_t extraFlags;  // For future use (e.g., to indicate if it's basic, nan, or lib data)
};
#pragma pack(pop)

std::ofstream logFile;

void writeToLog(const std::string& message) {
    logFile << message;
    logFile.flush();
}

template<typename T>
bool isNaN(T value) {
    return value != value;
}

struct FloatData {
    std::vector<char> rawData;
    size_t dataSize;
    FloatData(const std::vector<char>& data, size_t size) : rawData(data), dataSize(size) {}
};

long double bytesToLongDouble(const std::vector<char>& bytes, size_t dataSize) {
    if (dataSize == sizeof(float)) {
        float value;
        std::memcpy(&value, &bytes[0], sizeof(float));
        return static_cast<long double>(value);
    } else if (dataSize == sizeof(double)) {
        double value;
        std::memcpy(&value, &bytes[0], sizeof(double));
        return static_cast<long double>(value);
    } else if (dataSize == sizeof(long double)) {
        long double value;
        std::memcpy(&value, &bytes[0], sizeof(long double));
        return value;
    } else {
        return std::numeric_limits<long double>::quiet_NaN();
    }
}

std::vector<FloatData> readBinaryFile(const std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary);
    std::vector<FloatData> data;
    
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return data;
    }
    
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
    
    if (memcmp(header.magic, "SREF", 4) != 0) {
        std::cerr << "Invalid file format: " << filename << std::endl;
        return data;
    }
    
    for (uint32_t i = 0; i < header.elementCount; ++i) {
        std::vector<char> buffer(header.dataSize);
        file.read(&buffer[0], header.dataSize);
        if (file.fail()) {
            std::cerr << "Error reading file: " << filename << std::endl;
            data.clear();
            return data;
        }
        data.push_back(FloatData(buffer, header.dataSize));
    }

    return data;
}

template<typename T>
void compareFiles(const std::vector<std::string>& filenames, const std::string& type, int epsilonMultiple) {
    if (filenames.size() < 2) {
        std::cout << "Need at least two files to compare for " << type << ".\n";
        return;
    }

    std::vector<std::vector<FloatData> > allData;
    std::vector<FileHeader> headers;

    for (size_t i = 0; i < filenames.size(); ++i) {
        std::ifstream file(filenames[i].c_str(), std::ios::binary);
        if (!file) {
            std::cerr << "Error opening file: " << filenames[i] << std::endl;
            continue;
        }
        
        FileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
        
        if (memcmp(header.magic, "SREF", 4) != 0) {
            std::cerr << "Invalid file format: " << filenames[i] << std::endl;
            continue;
        }
        
        headers.push_back(header);
        file.close();
        
        std::vector<FloatData> data = readBinaryFile(filenames[i]);
        if (data.empty()) continue;
        
        allData.push_back(data);
    }

    if (allData.size() < 2) {
        std::cout << "No valid data to compare for " << type << ".\n";
        return;
    }

    long double epsilon = std::numeric_limits<long double>::epsilon() * epsilonMultiple;

    std::vector<int> exactMatches(allData.size(), 0);
    std::vector<int> nearMatches(allData.size(), 0);
    std::vector<int> differences(allData.size(), 0);

    std::ostringstream log;
    log << "Comparing " << type << " files:\n";
    for (size_t i = 0; i < filenames.size(); ++i) {
        std::string shortName = filenames[i].substr(filenames[i].find_last_of("/\\") + 1);
        log << "File: " << shortName << ", Data size: " << headers[i].dataSize << " bytes\n";
    }
    log << "Epsilon: " << std::scientific << std::setprecision(6) << epsilon 
        << " (" << epsilonMultiple << " * machine epsilon)\n";
    log << std::string(80, '-') << "\n\n";

    std::cout << log.str();
    writeToLog(log.str());
    log.str("");
    log.clear();

    for (size_t i = 0; i < allData[0].size(); ++i) {
        const FloatData& baselineValue = allData[0][i];
        long double baselineLongDouble = bytesToLongDouble(baselineValue.rawData, baselineValue.dataSize);
        bool hasDifference = false;
        bool hasNearMatch = false;

        for (size_t j = 1; j < allData.size(); ++j) {
            if (baselineValue.dataSize != allData[j][i].dataSize || 
                memcmp(&baselineValue.rawData[0], &allData[j][i].rawData[0], baselineValue.dataSize) != 0) {
                long double comparisonLongDouble = bytesToLongDouble(allData[j][i].rawData, allData[j][i].dataSize);
                if (std::isnan(baselineLongDouble) && std::isnan(comparisonLongDouble)) {
                    exactMatches[j]++;
                } else if (std::abs(comparisonLongDouble - baselineLongDouble) <= epsilon) {
                    nearMatches[j]++;
                    hasNearMatch = true;
                } else {
                    differences[j]++;
                    hasDifference = true;
                }
            } else {
                exactMatches[j]++;
            }
        }

        if (hasDifference || hasNearMatch) {
            log << "\n" << (hasDifference ? "Difference" : "Near match") << " at index " << i << ":\n";
            
            log << std::left << std::setw(25) << "Baseline"
                << std::setw(25) << std::scientific << std::setprecision(17) << baselineLongDouble << " ";
            for (size_t k = 0; k < baselineValue.dataSize; ++k) {
                log << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<int>(baselineValue.rawData[k] & 0xFF) << " ";
            }
            log << std::dec << std::setfill(' ') << "\n";

            for (size_t j = 1; j < allData.size(); ++j) {
                if (memcmp(&baselineValue.rawData[0], &allData[j][i].rawData[0], baselineValue.dataSize) != 0) {
                    std::string shortName = filenames[j].substr(filenames[j].find_last_of("/\\") + 1);
                    shortName = shortName.substr(0, 24);
                    long double comparisonLongDouble = bytesToLongDouble(allData[j][i].rawData, allData[j][i].dataSize);
                    log << std::left << std::setw(25) << shortName
                        << std::setw(25) << std::scientific << std::setprecision(17) << comparisonLongDouble << " ";
                    for (size_t k = 0; k < allData[j][i].dataSize; ++k) {
                        log << std::hex << std::setw(2) << std::setfill('0') 
                            << static_cast<int>(allData[j][i].rawData[k] & 0xFF) << " ";
                    }
                    log << std::dec << std::setfill(' ') << "\n";
                    
                    long double diff = comparisonLongDouble - baselineLongDouble;
                    log << std::left << std::setw(25) << "Difference"
                        << std::scientific << std::setprecision(10) << diff << "\n";
                }
            }
            log << "\n";

            writeToLog(log.str());
            log.str("");
            log.clear();
        }
    }

    std::ostringstream summary;
    summary << "Summary for " << type << ":\n";
    summary << std::left << std::setw(42) << "File"
            << std::setw(15) << "Exact Matches"
            << std::setw(15) << "Near Matches"
            << std::setw(15) << "Major Differences" << "\n";
    summary << std::string(87, '-') << "\n";

    for (size_t j = 1; j < allData.size(); ++j) {
        std::string shortName = filenames[j].substr(filenames[j].find_last_of("/\\") + 1);
        shortName = shortName.substr(0, 41);
        summary << std::left << std::setw(42) << shortName
                << std::setw(15) << exactMatches[j]
                << std::setw(15) << nearMatches[j]
                << std::setw(15) << differences[j] << "\n";
    }
    summary << std::string(87, '-') << "\n\n";

    std::cout << summary.str();
    writeToLog(summary.str());
}

void compareAllTypes(const std::vector<std::string>& basePaths) {
    const char* types[] = {"simple_basic", "simple_nan", "simple_lib", "double_basic", "double_nan", "double_lib"};
    const int numTypes = 6;
    
    std::cout << "Baseline: " << basePaths[0] << "\n\n";
    
    for (int i = 0; i < numTypes; ++i) {
        std::vector<std::string> filenames;
        for (size_t j = 0; j < basePaths.size(); ++j) {
            filenames.push_back(basePaths[j] + "_" + types[i] + ".bin");
        }
        
        if (std::string(types[i]).substr(0, 6) == "simple") {
            compareFiles<float>(filenames, types[i], 10000);
        } else {
            compareFiles<double>(filenames, types[i], 10000);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <basePath1> [basePath2] [basePath3] ...\n";
        return 1;
    }

    logFile.open("float_comparison.log");
    if (!logFile) {
        std::cerr << "Error opening log file" << std::endl;
        return 1;
    }

    std::vector<std::string> basePaths;
    for (int i = 1; i < argc; ++i) {
        basePaths.push_back(argv[i]);
    }

    compareAllTypes(basePaths);

    logFile.close();
    std::cout << "Detailed comparison results have been written to float_comparison.log" << std::endl;

    return 0;
}
