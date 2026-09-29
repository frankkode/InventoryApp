#pragma once

#include <string>
#include <vector>

#include "Product.h"
#include "Supplier.h"
#include "Transaction.h"

// Persistence: all file I/O for the three entity types, isolated here so
// neither the logic layer nor the CLI need to know the on-disk format.
//
// File format: one entity per line, fields separated by commas. A field is
// wrapped in double quotes only when it contains a comma, a quote, a
// backslash, or a newline; inside a quoted field, a backslash and a double
// quote are escaped as \\ and \", and an embedded newline as \n, the same
// escaping principle JSON string literals use. This keeps the common case
// (no special characters) readable as plain text, while a supplier name or
// note that happens to contain a comma still round-trips exactly.
//
// A missing file is not an error, load simply returns an empty vector,
// which is what a first run looks like. A line that fails to parse is
// skipped with a warning on stderr rather than aborting the whole load.
namespace Persistence {

std::vector<Product> loadProducts(const std::string& path);
std::vector<Supplier> loadSuppliers(const std::string& path);
std::vector<Transaction> loadTransactions(const std::string& path);

void saveProducts(const std::string& path, const std::vector<Product>& products);
void saveSuppliers(const std::string& path, const std::vector<Supplier>& suppliers);
void saveTransactions(const std::string& path, const std::vector<Transaction>& transactions);

}  // namespace Persistence
