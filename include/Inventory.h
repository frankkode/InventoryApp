#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Product.h"
#include "Supplier.h"
#include "Transaction.h"

// Inventory is the logic layer: the single place that validates input,
// mutates the in-memory product/supplier/transaction collections, and
// keeps them synchronized with disk through Persistence. The interface
// layer (main.cpp) never touches a std::vector or a data file directly,
// it only calls through here.
//
// Products are looked up by SKU through skuIndex_, an
// std::unordered_map<std::string, std::size_t> mapping each SKU to its
// position in products_. This is the one hash index in the design; it
// costs an additional lookup compared with a direct in-memory pointer,
// in exchange for O(1) average lookup by SKU instead of a linear scan.
// Suppliers are few enough in practice that they are searched linearly.
class Inventory {
public:
    enum class SortField { Name, Category, UnitPrice, QuantityOnHand, ReorderThreshold };

    Inventory(std::string productsPath, std::string suppliersPath, std::string transactionsPath);

    // Read-only views of the stored collections, for display only.
    const std::vector<Product>& products() const { return products_; }
    const std::vector<Supplier>& suppliers() const { return suppliers_; }
    const std::vector<Transaction>& transactions() const { return transactions_; }

    // Returns nullptr if no match exists.
    const Product* findProduct(const std::string& sku) const;
    const Supplier* findSupplier(const std::string& id) const;

    // Throws std::invalid_argument on an invalid or duplicate SKU, an
    // unknown supplierId (when non-empty), or a negative price/quantity/
    // reorder threshold.
    void addProduct(const Product& product);

    // Replaces every field of the product with the given SKU except the
    // SKU itself. Throws std::invalid_argument if the SKU does not
    // exist, an unknown supplierId is given, or a field is invalid.
    void editProduct(const std::string& sku, const Product& updated);

    // Throws std::invalid_argument if the SKU does not exist.
    void removeProduct(const std::string& sku);

    // Throws std::invalid_argument on an invalid or duplicate supplier id.
    void addSupplier(const Supplier& supplier);

    // Throws std::invalid_argument if the id does not exist, or if a
    // product still references it.
    void removeSupplier(const std::string& id);

    // Records a stock movement against an existing product and updates
    // its quantityOnHand. `quantity` is the non-negative amount the user
    // entered; this derives the signed quantityDelta from `type`:
    // StockIn adds it, StockOut subtracts it (refusing to take
    // quantityOnHand below zero), and Adjustment adds it as a positive
    // correction (for example, extra stock found during a count). A
    // decrease that is not a sale or shipment (shrinkage, a write-off)
    // is recorded as a StockOut with an explanatory note, so StockOut
    // remains the only type that ever lowers quantityOnHand.
    // Throws std::invalid_argument for an unknown SKU, a negative
    // quantity, or a StockOut that would leave quantityOnHand negative.
    Transaction recordMovement(const std::string& productSku, TransactionType type,
                                int quantity, const std::string& note = "");

    // Case-insensitive substring match against name and category.
    std::vector<Product> search(const std::string& query) const;

    // Returns a sorted copy for display; storage order and the SKU
    // index are left untouched.
    std::vector<Product> sortedProducts(SortField field, bool ascending = true) const;

private:
    std::string productsPath_;
    std::string suppliersPath_;
    std::string transactionsPath_;

    std::vector<Product> products_;
    std::vector<Supplier> suppliers_;
    std::vector<Transaction> transactions_;
    std::unordered_map<std::string, std::size_t> skuIndex_;

    void rebuildSkuIndex();
    void saveProducts() const;
    void saveSuppliers() const;
    void saveTransactions() const;
};
