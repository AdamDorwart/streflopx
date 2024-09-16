#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <limits>

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

template<typename T>
bool isNaN(T value) {
    return value != value;
}

template<typename T>
std::vector<T> readBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<T> data;
    
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
    
    if (header.dataSize != sizeof(T)) {
        std::cerr << "Mismatched data type size in file: " << filename << std::endl;
        return data;
    }
    
    // Reserve space in vector
    data.reserve(header.elementCount);

    // Read the data
    T value;
    for (uint32_t i = 0; i < header.elementCount; ++i) {
        file.read(reinterpret_cast<char*>(&value), header.dataSize);
        if (file.fail()) {
            std::cerr << "Error reading file: " << filename << std::endl;
            data.clear();
            return data;
        }
        data.push_back(value);
    }

    return data;
}

template<typename T>
void compareFiles(const std::vector<std::string>& filenames, const std::string& type, int epsilonMultiple) {
    if (filenames.size() < 2) {
        std::cout << "Need at least two files to compare for " << type << ".\n";
        return;
    }

    std::vector<std::vector<T> > allData;
    size_t dataSize = 0;

    for (size_t i = 0; i < filenames.size(); ++i) {
        std::vector<T> data = readBinaryFile<T>(filenames[i]);
        if (data.empty()) continue;
        
        if (dataSize == 0) {
            dataSize = data.size();
        } else if (data.size() != dataSize) {
            std::cout << "Error: File " << filenames[i] << " has a different number of elements.\n";
            return;
        }
        allData.push_back(data);
    }

    if (allData.size() < 2) {
        std::cout << "No valid data to compare for " << type << ".\n";
        return;
    }

    T epsilon = std::numeric_limits<T>::epsilon() * epsilonMultiple;

    std::cout << "\nComparing " << type << " files:\n";
    std::cout << "Epsilon: " << std::scientific << std::setprecision(6) << epsilon 
              << " (" << epsilonMultiple << " * machine epsilon)\n";
    std::cout << std::string(80, '-') << "\n";


    std::vector<int> exactMatches(allData.size(), 0);
    std::vector<int> nearMatches(allData.size(), 0);
    std::vector<int> differences(allData.size(), 0);

    for (size_t i = 0; i < dataSize; ++i) {
        T baselineValue = allData[0][i];
        bool hasDifference = false;
        bool hasNearMatch = false;

        for (size_t j = 1; j < allData.size(); ++j) {
            if (isNaN(baselineValue) && isNaN(allData[j][i])) {
                exactMatches[j]++;
            } else if (baselineValue == allData[j][i]) {
                exactMatches[j]++;
            } else if (std::abs(allData[j][i] - baselineValue) <= epsilon) {
                nearMatches[j]++;
                hasNearMatch = true;
            } else {
                differences[j]++;
                hasDifference = true;
            }
        }

        if (hasDifference || hasNearMatch) {
            std::cout << "\n" << (hasDifference ? "Difference" : "Near match") << " at index " << i << ":\n";
            
            // Print baseline
            std::cout << std::left << std::setw(25) << "Baseline"
                    << std::setw(25) << std::scientific << std::setprecision(17) << baselineValue
                    << "(0x" << std::hex << std::setw(16) << std::setfill('0') 
                    << *(reinterpret_cast<const uint64_t*>(&baselineValue)) << ")\n" << std::dec << std::setfill(' ');

            // Print other files
            for (size_t j = 1; j < allData.size(); ++j) {
                if (allData[j][i] != baselineValue) {
                    std::string shortName = filenames[j].substr(filenames[j].find_last_of("/\\") + 1);
                    shortName = shortName.substr(0, 24); // Truncate to 24 characters
                    std::cout << std::left << std::setw(25) << shortName
                            << std::setw(25) << std::scientific << std::setprecision(17) << allData[j][i]
                            << "(0x" << std::hex << std::setw(16) << std::setfill('0') 
                            << *(reinterpret_cast<const uint64_t*>(&allData[j][i])) << ")\n" << std::dec << std::setfill(' ');
                    
                    T diff = allData[j][i] - baselineValue;
                    std::cout << std::left << std::setw(20) << "Difference"
                            << std::scientific << std::setprecision(10) << diff << "\n";
                }
            }
            std::cout << "\n";
        }
    }

    std::cout << "Summary:\n";
    std::cout << std::left << std::setw(42) << "File"
              << std::setw(15) << "Exact Matches"
              << std::setw(15) << "Near Matches"
              << std::setw(15) << "Major Differences" << "\n";
    std::cout << std::string(87, '-') << "\n";

    for (size_t j = 1; j < allData.size(); ++j) {
        std::string shortName = filenames[j].substr(filenames[j].find_last_of("/\\") + 1);
        shortName = shortName.substr(0, 24); // Truncate to 24 characters
        std::cout << std::left << std::setw(42) << shortName
                  << std::setw(15) << exactMatches[j]
                  << std::setw(15) << nearMatches[j]
                  << std::setw(15) << differences[j] << "\n";
    }
    std::cout << std::string(87, '-') << "\n\n";
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
            compareFiles<float>(filenames, types[i], 1);
        } else {
            compareFiles<double>(filenames, types[i], 1);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <basePath1> [basePath2] [basePath3] ...\n";
        return 1;
    }

    std::vector<std::string> basePaths;
    for (int i = 1; i < argc; ++i) {
        basePaths.push_back(argv[i]);
    }

    compareAllTypes(basePaths);

    return 0;
}