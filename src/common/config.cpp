#include "common/config.h"
#include <fstream>
#include <sstream>

namespace hft {

bool Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            params_[key] = value;
        }
    }
    
    return true;
}

template<>
int Config::get(const std::string& key) const {
    auto it = params_.find(key);
    if (it != params_.end()) {
        return std::stoi(it->second);
    }
    return 0;
}

template<>
double Config::get(const std::string& key) const {
    auto it = params_.find(key);
    if (it != params_.end()) {
        return std::stod(it->second);
    }
    return 0.0;
}

template<>
std::string Config::get(const std::string& key) const {
    auto it = params_.find(key);
    if (it != params_.end()) {
        return it->second;
    }
    return "";
}

} // namespace hft
