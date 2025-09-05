//
// Created by ezzno on 2025/9/6.
//

#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <unordered_map>
#include <string>

#include "parser.h"


class namespace_ {
private:
    std::unordered_map<std::string, std::unique_ptr<FuncNode>> functions;

public:
    void register_function(std::string name, std::unique_ptr<FuncNode> func) {
        functions[std::move(name)] = std::move(func);
    }

    [[nodiscard]] const FuncNode* get_function(const std::string& name) const {
        auto it = functions.find(name);
        return (it != functions.end()) ? it->second.get() : nullptr;
    }
};

inline namespace_ global_namespace;


#endif //NAMESPACE_H
