#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

Config Config::load_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Confit file not found: " + path);
    }

    nlohmann::json j;
    in >> j;

    Config cfg;
    cfg.api_key_       = j.at("api_key").get<std::string>();
    cfg.client_id_     = j.at("client_id").get<std::string>();
    cfg.client_secret_ = j.at("client_secret").get<std::string>();
    cfg.tokens_        = j.at("tokens").get<std::vector<std::string>>();
    cfg.splits_        = j.at("splits").get<std::unordered_map<std::string,int>>();

    return cfg;
}
