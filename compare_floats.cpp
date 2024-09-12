#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>

template<typename T>
std::vector<T> readBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<T> data;
    T value;
    while (file.read(reinterpret_cast<char*>(&value), sizeof(T))) {
        data.push_back(value);
    }
    return data;
}

template<typename T>
void compareFiles(const std::string& file1, const std::string& file2, T epsilon) {
    auto data1 = readBinaryFile<T>(file1);
    auto data2 = readBinaryFile<T>(file2);

    if (data1.size() != data2.size()) {
        std::cout << "Error: Files have different number of elements.\n";
        return;
    }

    int exactMatches = 0;
    int nearMatches = 0;
    int differences = 0;

    for (size_t i = 0; i < data1.size(); ++i) {
        if (data1[i] == data2[i]) {
            exactMatches++;
        } else if (std::abs(data1[i] - data2[i]) <= epsilon) {
            nearMatches++;
        } else {
            differences++;
            std::cout << "Difference at index " << i << ": " 
                      << data1[i] << " vs " << data2[i] << "\n";
        }
    }

    std::cout << "Exact matches: " << exactMatches << "\n";
    std::cout << "Near matches: " << nearMatches << "\n";
    std::cout << "Significant differences: " << differences << "\n";
}

int main() {
    // Compare Simple precision files
    std::cout << "Comparing Simple precision basic operations:\n";
    compareFiles<float>("arm64_neon_simple_basic.bin", "arm64_soft_simple_basic.bin", 1e-6f);

    std::cout << "\nComparing Simple precision NaN operations:\n";
    compareFiles<float>("arm64_neon_simple_nan.bin", "arm64_soft_simple_nan.bin", 1e-6f);

    std::cout << "\nComparing Simple precision math library operations:\n";
    compareFiles<float>("arm64_neon_simple_lib.bin", "arm64_soft_simple_lib.bin", 1e-6f);

    // Compare Double precision files
    std::cout << "\nComparing Double precision basic operations:\n";
    compareFiles<double>("arm64_neon_double_basic.bin", "arm64_soft_double_basic.bin", 1e-15);

    std::cout << "\nComparing Double precision NaN operations:\n";
    compareFiles<double>("arm64_neon_double_nan.bin", "arm64_soft_double_nan.bin", 1e-15);

    std::cout << "\nComparing Double precision math library operations:\n";
    compareFiles<double>("arm64_neon_double_lib.bin", "arm64_soft_double_lib.bin", 1e-15);

    return 0;
}
