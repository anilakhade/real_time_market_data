#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class Config {
public:
    // Load settings from json file
    static Config load_from_file(const std::string& path);

    // Accessors (read-only)
    const std::string& api_key() const {return api_key_; }
    const std::string& client_code() const {return client_id_; }
    const std::string& client_secret() const {return client_secret_; }
    const std::vector<std::string>& token() const {return tokens_; }
    const std::unordered_map<std::string, int>& splits() const {return splits_; }

private:
    // private ctor enforce factory method
    Config() = default;

    std::string api_key_;
    std::string client_id_;
    std::string client_secret_;
    std::vector<std::string> tokens_;
    std::unordered_map<std::string, int> splits_;

};
