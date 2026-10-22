#include "Inventory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <stdexcept>

#include "Persistence.h"

namespace {

// Returns a lowercase copy of a string, so we can compare without caring about case.
std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Returns the current date and time as "YYYY-MM-DD HH:MM:SS" for new transactions.
std::string currentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buffer);
}

}  // namespace

// Loads the three data files into memory and rebuilds the SKU lookup index.
Inventory::Inventory(std::string productsPath, std::string suppliersPath, std::string transactionsPath)
    : productsPath_(std::move(productsPath)),
      suppliersPath_(std::move(suppliersPath)),
      transactionsPath_(std::move(transactionsPath)),
      products_(Persistence::loadProducts(productsPath_)),
      suppliers_(Persistence::loadSuppliers(suppliersPath_)),
      transactions_(Persistence::loadTransactions(transactionsPath_)) {
    rebuildSkuIndex();
}

// Wipes the SKU index and rebuilds it from the current product list.
void Inventory::rebuildSkuIndex() {
    skuIndex_.clear();
    for (std::size_t i = 0; i < products_.size(); ++i) {
        skuIndex_[products_[i].sku] = i;
    }
}

// Returns a pointer to the product with this SKU, or nullptr if there is none.
const Product* Inventory::findProduct(const std::string& sku) const {
    auto it = skuIndex_.find(sku);
    if (it == skuIndex_.end()) {
        return nullptr;
    }
    return &products_[it->second];
}

// Returns a pointer to the supplier with this ID, or nullptr if there is none.
const Supplier* Inventory::findSupplier(const std::string& id) const {
    for (const auto& supplier : suppliers_) {
        if (supplier.id == id) {
            return &supplier;
        }
    }
    return nullptr;
}

// Validates a new product, stores it, and saves the products file.
void Inventory::addProduct(const Product& product) {
    if (!Product::isValidSku(product.sku)) {
        throw std::invalid_argument("invalid SKU");
    }
    if (skuIndex_.count(product.sku) != 0) {
        throw std::invalid_argument("duplicate SKU: " + product.sku);
    }
    if (!product.supplierId.empty() && findSupplier(product.supplierId) == nullptr) {
        throw std::invalid_argument("unknown supplierId: " + product.supplierId);
    }
    if (!std::isfinite(product.unitPrice) || product.unitPrice < 0.0 ||
        product.quantityOnHand < 0 || product.reorderThreshold < 0) {
        throw std::invalid_argument(
            "price must be a finite, non-negative number, and quantity and reorder threshold "
            "must be non-negative");
    }
    products_.push_back(product);
    skuIndex_[product.sku] = products_.size() - 1;
    saveProducts();
}

// Replaces every field of a product except its SKU, then saves the file.
void Inventory::editProduct(const std::string& sku, const Product& updated) {
    auto it = skuIndex_.find(sku);
    if (it == skuIndex_.end()) {
        throw std::invalid_argument("unknown SKU: " + sku);
    }
    if (updated.sku != sku) {
        throw std::invalid_argument("editProduct cannot change the SKU");
    }
    if (!updated.supplierId.empty() && findSupplier(updated.supplierId) == nullptr) {
        throw std::invalid_argument("unknown supplierId: " + updated.supplierId);
    }
    if (!std::isfinite(updated.unitPrice) || updated.unitPrice < 0.0 ||
        updated.quantityOnHand < 0 || updated.reorderThreshold < 0) {
        throw std::invalid_argument(
            "price must be a finite, non-negative number, and quantity and reorder threshold "
            "must be non-negative");
    }
    products_[it->second] = updated;
    saveProducts();
}

// Deletes the product with this SKU and saves the products file.
void Inventory::removeProduct(const std::string& sku) {
    auto it = skuIndex_.find(sku);
    if (it == skuIndex_.end()) {
        throw std::invalid_argument("unknown SKU: " + sku);
    }
    products_.erase(products_.begin() + static_cast<std::ptrdiff_t>(it->second));
    rebuildSkuIndex();
    saveProducts();
}

// Validates a new supplier, stores it, and saves the suppliers file.
void Inventory::addSupplier(const Supplier& supplier) {
    if (!Supplier::isValidSupplierId(supplier.id)) {
        throw std::invalid_argument("invalid supplier id");
    }
    if (findSupplier(supplier.id) != nullptr) {
        throw std::invalid_argument("duplicate supplier id: " + supplier.id);
    }
    suppliers_.push_back(supplier);
    saveSuppliers();
}

// Deletes a supplier, unless a product still references it.
void Inventory::removeSupplier(const std::string& id) {
    auto it = std::find_if(suppliers_.begin(), suppliers_.end(),
                            [&id](const Supplier& s) { return s.id == id; });
    if (it == suppliers_.end()) {
        throw std::invalid_argument("unknown supplier id: " + id);
    }
    bool inUse = std::any_of(products_.begin(), products_.end(),
                              [&id](const Product& p) { return p.supplierId == id; });
    if (inUse) {
        throw std::invalid_argument("supplier " + id + " is still referenced by a product");
    }
    suppliers_.erase(it);
    saveSuppliers();
}

// Logs a stock movement, updates the product's quantity, and saves both files.
Transaction Inventory::recordMovement(const std::string& productSku, TransactionType type,
                                       int quantity, const std::string& note) {
    auto it = skuIndex_.find(productSku);
    if (it == skuIndex_.end()) {
        throw std::invalid_argument("unknown SKU: " + productSku);
    }
    if (quantity < 0) {
        throw std::invalid_argument("quantity must be non-negative");
    }

    Product& product = products_[it->second];
    int delta = 0;
    switch (type) {
        case TransactionType::StockIn:
        case TransactionType::Adjustment:
            delta = quantity;
            break;
        case TransactionType::StockOut:
            delta = -quantity;
            break;
    }
    if (product.quantityOnHand + delta < 0) {
        throw std::invalid_argument("stock-out would leave quantityOnHand negative for " + productSku);
    }
    product.quantityOnHand += delta;

    Transaction transaction;
    transaction.id = productSku + "-" + std::to_string(transactions_.size() + 1);
    transaction.timestamp = currentTimestamp();
    transaction.productSku = productSku;
    transaction.type = type;
    transaction.quantityDelta = delta;
    transaction.note = note;

    transactions_.push_back(transaction);
    saveProducts();
    saveTransactions();
    return transaction;
}

// Finds products whose name or category contains the query, ignoring case.
std::vector<Product> Inventory::search(const std::string& query) const {
    std::string needle = toLower(query);
    std::vector<Product> results;
    for (const auto& product : products_) {
        if (toLower(product.name).find(needle) != std::string::npos ||
            toLower(product.category).find(needle) != std::string::npos) {
            results.push_back(product);
        }
    }
    return results;
}

// Returns a sorted copy of the product list, leaving the stored order alone.
std::vector<Product> Inventory::sortedProducts(SortField field, bool ascending) const {
    std::vector<Product> result = products_;
    std::sort(result.begin(), result.end(), [field, ascending](const Product& a, const Product& b) {
        switch (field) {
            case SortField::Name:
                return ascending ? (a.name < b.name) : (b.name < a.name);
            case SortField::Category:
                return ascending ? (a.category < b.category) : (b.category < a.category);
            case SortField::UnitPrice:
                return ascending ? (a.unitPrice < b.unitPrice) : (b.unitPrice < a.unitPrice);
            case SortField::QuantityOnHand:
                return ascending ? (a.quantityOnHand < b.quantityOnHand) : (b.quantityOnHand < a.quantityOnHand);
            case SortField::ReorderThreshold:
                return ascending ? (a.reorderThreshold < b.reorderThreshold) : (b.reorderThreshold < a.reorderThreshold);
        }
        return false;
    });
    return result;
}

// Each of these helpers writes its collection back to its data file.
void Inventory::saveProducts() const { Persistence::saveProducts(productsPath_, products_); }
void Inventory::saveSuppliers() const { Persistence::saveSuppliers(suppliersPath_, suppliers_); }
void Inventory::saveTransactions() const { Persistence::saveTransactions(transactionsPath_, transactions_); }
