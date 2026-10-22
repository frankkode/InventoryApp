#pragma once

#include <string>
#include <vector>

#include "Product.h"
#include "Supplier.h"
#include "Transaction.h"

// Persistence: all file I/O for the three entity types, isolated here so
// neither the logic layer nor the CLI need to know the on-disk format.
//
// File format: one entity per line, comma-separated fields. A field is
// quoted only when it contains a comma, quote, backslash, or newline;
// inside quotes, \\ and \" escape themselves and \n is a literal
// newline, the same idea JSON string literals use. Plain fields stay
// unquoted so the common case still reads as plain text.
//
// A missing file just loads as an empty vector (a normal first run), and
// a line that fails to parse is skipped with a warning instead of
// aborting the whole load.
namespace Persistence {

std::vector<Product> loadProducts(const std::string& path);
std::vector<Supplier> loadSuppliers(const std::string& path);
std::vector<Transaction> loadTransactions(const std::string& path);

void saveProducts(const std::string& path, const std::vector<Product>& products);
void saveSuppliers(const std::string& path, const std::vector<Supplier>& suppliers);
void saveTransactions(const std::string& path, const std::vector<Transaction>& transactions);

}  // namespace Persistence
