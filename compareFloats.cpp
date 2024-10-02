#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

#pragma pack(push, 1)
struct FileHeader {
    char magic[4];
    uint32_t version;
    uint32_t dataType;
    uint32_t dataSize;
    uint32_t elementCount;
    uint32_t extraFlags;
};
#pragma pack(pop)

std::ofstream logFile;

void writeToLog(const std::string& message) {
    logFile << message;
    logFile.flush();
}

template<typename T>
bool isNaN(T value) {
    return std::isnan(value);
}

union FloatInt {
    float f;
    uint32_t i;
};

union DoubleInt {
    double d;
    uint64_t i;
};

struct FloatData {
    std::vector<char> rawData;
    size_t dataSize;
    FloatData(const std::vector<char>& data, size_t size) : rawData(data), dataSize(size) {}
};

// Endian-aware read function
template<typename T>
T readValue(std::ifstream& file) {
    T value;
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    
    // Check endianness
    static const int endianCheck = 1;
    if (*reinterpret_cast<const char*>(&endianCheck) == 1) {
        // System is little-endian, reverse bytes
        char* start = reinterpret_cast<char*>(&value);
        std::reverse(start, start + sizeof(T));
    }
    
    return value;
}

std::vector<FloatData> readBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
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
        file.read(buffer.data(), header.dataSize);
        if (file.fail()) {
            std::cerr << "Error reading file: " << filename << std::endl;
            data.clear();
            return data;
        }
        
        // Reverse bytes if system is little-endian
        static const int endianCheck = 1;
        if (*reinterpret_cast<const char*>(&endianCheck) == 1) {
            std::reverse(buffer.begin(), buffer.end());
        }
        
        data.push_back(FloatData(buffer, header.dataSize));
    }

    return data;
}

template<typename T>
T bytesToFloat(const std::vector<char>& bytes) {
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}

int64_t ulpDiff(float a, float b) {
    if (a == b) return 0;
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<int64_t>::max();
    
    FloatInt ai, bi;
    ai.f = a;
    bi.f = b;
    
    if ((ai.i & 0x80000000) != (bi.i & 0x80000000)) {
        return std::numeric_limits<int64_t>::max();
    }
    
    return std::abs(static_cast<int64_t>(ai.i) - static_cast<int64_t>(bi.i));
}

int64_t ulpDiff(double a, double b) {
    if (a == b) return 0;
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<int64_t>::max();
    
    DoubleInt ai, bi;
    ai.d = a;
    bi.d = b;
    
    if ((ai.i & 0x8000000000000000ULL) != (bi.i & 0x8000000000000000ULL)) {
        return std::numeric_limits<int64_t>::max();
    }
    
    return std::abs(static_cast<int64_t>(ai.i) - static_cast<int64_t>(bi.i));
}

std::string floatToHex(float value) {
    FloatInt fi;
    fi.f = value;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << fi.i;
    return oss.str();
}

std::string doubleToHex(double value) {
    DoubleInt di;
    di.d = value;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << di.i;
    return oss.str();
}

std::string floatDiffDescription(const FloatData& a, const FloatData& b) {
    std::ostringstream oss;
    
    if (a.dataSize == sizeof(float)) {
        float aFloat = bytesToFloat<float>(a.rawData);
        float bFloat = bytesToFloat<float>(b.rawData);
        int64_t ulpDifference = ulpDiff(aFloat, bFloat);
        
        oss << "Float difference: " << std::scientific << std::setprecision(9)
            << aFloat << " vs " << bFloat << "\n"
            << "ULP difference: " << ulpDifference << "\n"
            << "Hex representations: " << floatToHex(aFloat) << " vs " << floatToHex(bFloat);
    }
    else if (a.dataSize == sizeof(double)) {
        double aDouble = bytesToFloat<double>(a.rawData);
        double bDouble = bytesToFloat<double>(b.rawData);
        int64_t ulpDifference = ulpDiff(aDouble, bDouble);
        
        oss << "Double difference: " << std::scientific << std::setprecision(17)
            << aDouble << " vs " << bDouble << "\n"
            << "ULP difference: " << ulpDifference << "\n"
            << "Hex representations: " << doubleToHex(aDouble) << " vs " << doubleToHex(bDouble);
    }
    else {
        oss << "Unsupported float size for detailed comparison";
    }
    
    return oss.str();
}

bool compareFloats(const FloatData& a, const FloatData& b, int maxUlpDiff, std::string& diffDescription) {
    if (a.dataSize != b.dataSize) {
        diffDescription = "Data size mismatch";
        return false;
    }

    if (a.dataSize == sizeof(float)) {
        float aFloat = bytesToFloat<float>(a.rawData);
        float bFloat = bytesToFloat<float>(b.rawData);
        
        if (std::isnan(aFloat) && std::isnan(bFloat)) return true;
        if (std::isnan(aFloat) || std::isnan(bFloat)) {
            diffDescription = "NaN mismatch";
            return false;
        }
        
        int64_t diff = ulpDiff(aFloat, bFloat);
        if (diff <= maxUlpDiff) return true;
        
        diffDescription = floatDiffDescription(a, b);
        return false;
    }
    else if (a.dataSize == sizeof(double)) {
        double aDouble = bytesToFloat<double>(a.rawData);
        double bDouble = bytesToFloat<double>(b.rawData);
        
        if (std::isnan(aDouble) && std::isnan(bDouble)) return true;
        if (std::isnan(aDouble) || std::isnan(bDouble)) {
            diffDescription = "NaN mismatch";
            return false;
        }
        
        int64_t diff = ulpDiff(aDouble, bDouble);
        if (diff <= maxUlpDiff) return true;
        
        diffDescription = floatDiffDescription(a, b);
        return false;
    }
    
    diffDescription = "Unsupported float size";
    return false;
}

template<typename T>
void compareFiles(const std::vector<std::string>& filenames, const std::string& type, int maxUlpDiff) {
    if (filenames.size() < 2) {
        std::cout << "Need at least two files to compare for " << type << ".\n";
        return;
    }

    std::vector<std::vector<FloatData>> allData;
    std::vector<FileHeader> headers;

    for (const auto& filename : filenames) {
        std::vector<FloatData> data = readBinaryFile(filename);
        if (data.empty()) {
            std::cout << "Skipping file due to read error: " << filename << std::endl;
            continue;
        }
        
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error opening file: " << filename << std::endl;
            continue;
        }
        
        FileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
        file.close();
        
        headers.push_back(header);
        allData.push_back(data);
    }

    if (allData.size() < 2) {
        std::cout << "No valid data to compare for " << type << ".\n";
        return;
    }

    std::vector<int> exactMatches(allData.size(), 0);
    std::vector<int> differences(allData.size(), 0);

    std::ostringstream log;
    log << "Comparing " << type << " files:\n";
    for (size_t i = 0; i < filenames.size(); ++i) {
        std::string shortName = filenames[i].substr(filenames[i].find_last_of("/\\") + 1);
        log << "File: " << shortName << ", Data size: " << headers[i].dataSize << " bytes\n";
    }
    log << "Max ULP difference: " << maxUlpDiff << "\n";
    log << std::string(80, '-') << "\n\n";

    std::cout << log.str();
    writeToLog(log.str());
    log.str("");
    log.clear();

    for (size_t i = 0; i < allData[0].size(); ++i) {
        const FloatData& baselineValue = allData[0][i];
        bool hasDifference = false;

        for (size_t j = 1; j < allData.size(); ++j) {
            std::string diffDescription;
            if (compareFloats(baselineValue, allData[j][i], maxUlpDiff, diffDescription)) {
                exactMatches[j]++;
            } else {
                differences[j]++;
                hasDifference = true;
                
                log << "Difference at element " << i << " between file 0 and file " << j << ":\n";
                log << diffDescription << "\n\n";
            }
        }

        if (hasDifference) {
            std::cout << log.str();
            writeToLog(log.str());
            log.str("");
            log.clear();
        }
    }
    
    std::ostringstream summary;
    summary << "Summary for " << type << ":\n";
    summary << std::left << std::setw(42) << "File"
            << std::setw(15) << "Exact Matches"
            << std::setw(15) << "Differences" << "\n";
    summary << std::string(72, '-') << "\n";

    for (size_t j = 1; j < allData.size(); ++j) {
        std::string shortName = filenames[j].substr(filenames[j].find_last_of("/\\") + 1);
        shortName = shortName.substr(0, 41);
        summary << std::left << std::setw(42) << shortName
                << std::setw(15) << exactMatches[j]
                << std::setw(15) << differences[j] << "\n";
    }
    summary << std::string(72, '-') << "\n\n";

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
            std::string filename = basePaths[j] + "_" + types[i] + ".bin";
            std::ifstream file(filename);
            if (file.good()) {
                filenames.push_back(filename);
                file.close();
            } else {
                std::cout << "Warning: File not found - " << filename << std::endl;
            }
        }
        
        if (filenames.size() < 2) {
            std::cout << "Not enough valid files to compare for " << types[i] << ". Skipping.\n\n";
            continue;
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
