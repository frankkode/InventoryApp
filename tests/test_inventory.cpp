// Plain-assertion test suite for the logic and persistence layers. No
// external framework: check() records a pass/fail and prints which, and
// main() reports a final tally with a non-zero exit code on any failure
// so `ctest` (and a CI run, if one is ever added) can detect it.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Inventory.h"
#include "Persistence.h"

namespace {

int checksRun = 0;
int checksPassed = 0;

void check(bool condition, const std::string& description) {
    ++checksRun;
    if (condition) {
        ++checksPassed;
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
    }
}

// Removes any leftover test data files so each test starts from a known,
// empty state, matching Persistence's documented "missing file -> empty
// vector" behavior.
void removeTestFiles(const std::string& productsPath, const std::string& suppliersPath,
                      const std::string& transactionsPath) {
    std::remove(productsPath.c_str());
    std::remove(suppliersPath.c_str());
    std::remove(transactionsPath.c_str());
}

Product makeProduct(const std::string& sku, const std::string& name = "Widget",
                     double price = 9.99, int quantity = 10, int reorder = 2,
                     const std::string& supplierId = "") {
    Product p;
    p.sku = sku;
    p.name = name;
    p.category = "Tools";
    p.unitPrice = price;
    p.quantityOnHand = quantity;
    p.reorderThreshold = reorder;
    p.supplierId = supplierId;
    return p;
}

void testEmptyInventoryOnMissingFiles() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    check(inv.products().empty(), "a missing products file loads as an empty inventory");
    check(inv.suppliers().empty(), "a missing suppliers file loads as an empty inventory");
    check(inv.transactions().empty(), "a missing transactions file loads as an empty inventory");
}

void testAddProductValidation() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");

    inv.addProduct(makeProduct("SKU1"));
    check(inv.findProduct("SKU1") != nullptr, "a valid product can be added and found by SKU");

    bool rejectedDuplicate = false;
    try {
        inv.addProduct(makeProduct("SKU1"));
    } catch (const std::invalid_argument&) {
        rejectedDuplicate = true;
    }
    check(rejectedDuplicate, "adding a duplicate SKU is rejected");

    bool rejectedBlankSku = false;
    try {
        inv.addProduct(makeProduct(""));
    } catch (const std::invalid_argument&) {
        rejectedBlankSku = true;
    }
    check(rejectedBlankSku, "adding a blank SKU is rejected");

    bool rejectedUnknownSupplier = false;
    try {
        inv.addProduct(makeProduct("SKU2", "Gadget", 5.0, 3, 1, "NOSUCHSUPPLIER"));
    } catch (const std::invalid_argument&) {
        rejectedUnknownSupplier = true;
    }
    check(rejectedUnknownSupplier, "adding a product with an unknown supplierId is rejected");
}

void testEditAndRemoveProduct() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    inv.addProduct(makeProduct("SKU1", "Widget", 9.99, 10, 2));

    Product updated = *inv.findProduct("SKU1");
    updated.name = "Deluxe Widget";
    updated.quantityOnHand = 20;
    inv.editProduct("SKU1", updated);
    check(inv.findProduct("SKU1")->name == "Deluxe Widget", "editProduct updates the stored fields");
    check(inv.findProduct("SKU1")->quantityOnHand == 20, "editProduct updates quantityOnHand");

    bool rejectedSkuChange = false;
    try {
        Product renamed = updated;
        renamed.sku = "SKU2";
        inv.editProduct("SKU1", renamed);
    } catch (const std::invalid_argument&) {
        rejectedSkuChange = true;
    }
    check(rejectedSkuChange, "editProduct rejects changing the SKU");

    inv.removeProduct("SKU1");
    check(inv.findProduct("SKU1") == nullptr, "removeProduct removes the product");

    bool rejectedUnknownRemove = false;
    try {
        inv.removeProduct("SKU1");
    } catch (const std::invalid_argument&) {
        rejectedUnknownRemove = true;
    }
    check(rejectedUnknownRemove, "removing an unknown SKU is rejected");
}

void testSupplierLifecycleAndProtection() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");

    Supplier s;
    s.id = "SUP1";
    s.name = "Acme";
    s.contact = "acme@example.com";
    inv.addSupplier(s);
    check(inv.findSupplier("SUP1") != nullptr, "a valid supplier can be added and found");

    bool rejectedDuplicateSupplier = false;
    try {
        inv.addSupplier(s);
    } catch (const std::invalid_argument&) {
        rejectedDuplicateSupplier = true;
    }
    check(rejectedDuplicateSupplier, "adding a duplicate supplier id is rejected");

    inv.addProduct(makeProduct("SKU1", "Widget", 9.99, 10, 2, "SUP1"));

    bool rejectedInUseRemoval = false;
    try {
        inv.removeSupplier("SUP1");
    } catch (const std::invalid_argument&) {
        rejectedInUseRemoval = true;
    }
    check(rejectedInUseRemoval, "removing a supplier still referenced by a product is rejected");

    inv.removeProduct("SKU1");
    inv.removeSupplier("SUP1");
    check(inv.findSupplier("SUP1") == nullptr, "a supplier no longer referenced can be removed");
}

void testStockMovements() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    inv.addProduct(makeProduct("SKU1", "Widget", 9.99, 10, 2));

    inv.recordMovement("SKU1", TransactionType::StockIn, 5, "restock");
    check(inv.findProduct("SKU1")->quantityOnHand == 15, "stock-in increases quantityOnHand");

    inv.recordMovement("SKU1", TransactionType::StockOut, 3, "sale");
    check(inv.findProduct("SKU1")->quantityOnHand == 12, "stock-out decreases quantityOnHand");

    inv.recordMovement("SKU1", TransactionType::Adjustment, 2, "count correction");
    check(inv.findProduct("SKU1")->quantityOnHand == 14, "adjustment applies as a positive correction");

    check(inv.transactions().size() == 3, "each movement is logged as a transaction");

    bool rejectedNegativeQuantity = false;
    try {
        inv.recordMovement("SKU1", TransactionType::StockIn, -1, "bad input");
    } catch (const std::invalid_argument&) {
        rejectedNegativeQuantity = true;
    }
    check(rejectedNegativeQuantity, "a negative movement quantity is rejected");

    bool rejectedOverdraw = false;
    try {
        inv.recordMovement("SKU1", TransactionType::StockOut, 1000, "too many");
    } catch (const std::invalid_argument&) {
        rejectedOverdraw = true;
    }
    check(rejectedOverdraw, "a stock-out larger than quantityOnHand is rejected");
    check(inv.findProduct("SKU1")->quantityOnHand == 14, "a rejected stock-out leaves quantityOnHand unchanged");

    bool rejectedUnknownSku = false;
    try {
        inv.recordMovement("NOPE", TransactionType::StockIn, 1, "n/a");
    } catch (const std::invalid_argument&) {
        rejectedUnknownSku = true;
    }
    check(rejectedUnknownSku, "recording a movement against an unknown SKU is rejected");
}

void testSearchAndSort() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");

    Product hammer = makeProduct("SKU1", "Hammer", 12.0, 5, 2);
    hammer.category = "Hand Tools";
    Product screwdriver = makeProduct("SKU2", "Screwdriver", 6.0, 20, 5);
    screwdriver.category = "Hand Tools";
    Product drill = makeProduct("SKU3", "Power Drill", 18.0, 3, 1);
    drill.category = "Power Tools";

    inv.addProduct(hammer);
    inv.addProduct(screwdriver);
    inv.addProduct(drill);

    auto byName = inv.search("SCREW");
    check(byName.size() == 1 && byName.front().sku == "SKU2",
          "search matches by name, case-insensitively");

    auto byCategory = inv.search("hand");
    check(byCategory.size() == 2, "search also matches by category, returning every Hand Tools product");

    auto byPriceAsc = inv.sortedProducts(Inventory::SortField::UnitPrice, true);
    check(byPriceAsc.front().sku == "SKU2" && byPriceAsc.back().sku == "SKU3",
          "sortedProducts orders ascending by unit price");

    auto byPriceDesc = inv.sortedProducts(Inventory::SortField::UnitPrice, false);
    check(byPriceDesc.front().sku == "SKU3" && byPriceDesc.back().sku == "SKU2",
          "sortedProducts orders descending by unit price");

    check(inv.products().size() == 3, "sortedProducts does not reorder underlying storage");
}

void testPersistenceRoundTrip() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    {
        Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
        Supplier s;
        s.id = "SUP1";
        s.name = "Acme, Inc.";                          // embedded comma exercises quoting
        s.contact = "Say \"hi\" at acme@example.com";    // embedded quote
        inv.addSupplier(s);
        inv.addProduct(makeProduct("SKU1", "Widget", 9.99, 10, 2, "SUP1"));
        inv.recordMovement("SKU1", TransactionType::StockIn, 5, "restock, first batch");
    }

    Inventory reloaded("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    check(reloaded.products().size() == 1, "products survive a save/reload round trip");
    check(reloaded.suppliers().size() == 1, "suppliers survive a save/reload round trip");
    check(reloaded.transactions().size() == 1, "transactions survive a save/reload round trip");
    check(reloaded.findSupplier("SUP1") != nullptr &&
              reloaded.findSupplier("SUP1")->name == "Acme, Inc.",
          "a supplier name containing a comma round-trips exactly");
    check(reloaded.findSupplier("SUP1")->contact == "Say \"hi\" at acme@example.com",
          "a field containing embedded quotes round-trips exactly");
    check(reloaded.findProduct("SKU1")->quantityOnHand == 15,
          "quantityOnHand reflects the recorded movement after reload");

    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
}

void testMalformedLineIsSkipped() {
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    {
        // One well-formed line and one line with too few fields.
        std::ofstream out("t_products.dat");
        out << "SKU1,Widget,Tools,9.99,10,2,,\n";
        out << "this line is not a valid product record\n";
    }
    Inventory inv("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    check(inv.products().size() == 1, "a malformed line is skipped rather than aborting the whole load");
    check(inv.findProduct("SKU1") != nullptr, "the well-formed line still loads despite the bad one");
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
}

}  // namespace

int main() {
    testEmptyInventoryOnMissingFiles();
    testAddProductValidation();
    testEditAndRemoveProduct();
    testSupplierLifecycleAndProtection();
    testStockMovements();
    testSearchAndSort();
    testPersistenceRoundTrip();
    testMalformedLineIsSkipped();

    std::cout << "\n" << checksPassed << "/" << checksRun << " checks passed.\n";
    removeTestFiles("t_products.dat", "t_suppliers.dat", "t_transactions.dat");
    return checksPassed == checksRun ? 0 : 1;
}
