#include "Persistence.h"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

bool needsQuoting(const std::string& field) {
    return field.find_first_of(",\"\\\n") != std::string::npos;
}

std::string escapeField(const std::string& field) {
    if (!needsQuoting(field)) {
        return field;
    }
    std::string out = "\"";
    for (char c : field) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out += c;
        }
    }
    out += "\"";
    return out;
}

std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < line.size()) {
                char next = line[++i];
                if (next == 'n') {
                    current += '\n';
                } else {
                    current += next;
                }
            } else if (c == '"') {
                inQuotes = false;
            } else {
                current += c;
            }
        } else {
            if (c == '"' && current.empty()) {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
    }
    if (inQuotes) {
        throw std::runtime_error("unterminated quoted field");
    }
    fields.push_back(current);
    return fields;
}

std::string joinFields(const std::vector<std::string>& fields) {
    std::string line;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            line += ',';
        }
        line += escapeField(fields[i]);
    }
    return line;
}

template <typename T, typename ParseFn>
std::vector<T> loadEntities(const std::string& path, ParseFn parseLine) {
    std::vector<T> result;
    std::ifstream in(path);
    if (!in) {
        return result;
    }
    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }
        T entity;
        if (parseLine(line, entity)) {
            result.push_back(std::move(entity));
        } else {
            std::cerr << "Warning: skipping malformed line " << lineNumber
                      << " in " << path << "\n";
        }
    }
    return result;
}

template <typename T, typename FormatFn>
void saveEntities(const std::string& path, const std::vector<T>& entities, FormatFn formatLine) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open " + path + " for writing");
    }
    for (const auto& entity : entities) {
        out << formatLine(entity) << '\n';
    }
}

}  // namespace

namespace Persistence {

std::vector<Product> loadProducts(const std::string& path) {
    return loadEntities<Product>(path, [](const std::string& line, Product& p) {
        auto f = splitFields(line);
        if (f.size() != 8) {
            return false;
        }
        try {
            p.sku = f[0];
            p.name = f[1];
            p.category = f[2];
            p.unitPrice = std::stod(f[3]);
            p.quantityOnHand = std::stoi(f[4]);
            p.reorderThreshold = std::stoi(f[5]);
            p.supplierId = f[6];
            p.imagePath = f[7];
        } catch (const std::exception&) {
            return false;
        }
        return true;
    });
}

void saveProducts(const std::string& path, const std::vector<Product>& products) {
    saveEntities(path, products, [](const Product& p) {
        return joinFields({
            p.sku, p.name, p.category,
            std::to_string(p.unitPrice), std::to_string(p.quantityOnHand),
            std::to_string(p.reorderThreshold), p.supplierId, p.imagePath,
        });
    });
}

std::vector<Supplier> loadSuppliers(const std::string& path) {
    return loadEntities<Supplier>(path, [](const std::string& line, Supplier& s) {
        auto f = splitFields(line);
        if (f.size() != 3) {
            return false;
        }
        s.id = f[0];
        s.name = f[1];
        s.contact = f[2];
        return true;
    });
}

void saveSuppliers(const std::string& path, const std::vector<Supplier>& suppliers) {
    saveEntities(path, suppliers, [](const Supplier& s) {
        return joinFields({s.id, s.name, s.contact});
    });
}

std::vector<Transaction> loadTransactions(const std::string& path) {
    return loadEntities<Transaction>(path, [](const std::string& line, Transaction& t) {
        auto f = splitFields(line);
        if (f.size() != 6) {
            return false;
        }
        try {
            t.id = f[0];
            t.timestamp = f[1];
            t.productSku = f[2];
            t.type = transactionTypeFromString(f[3]);
            t.quantityDelta = std::stoi(f[4]);
            t.note = f[5];
        } catch (const std::exception&) {
            return false;
        }
        return true;
    });
}

void saveTransactions(const std::string& path, const std::vector<Transaction>& transactions) {
    saveEntities(path, transactions, [](const Transaction& t) {
        return joinFields({
            t.id, t.timestamp, t.productSku, toString(t.type),
            std::to_string(t.quantityDelta), t.note,
        });
    });
}

}  // namespace Persistence
