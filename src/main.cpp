// Interface layer: a text menu over Inventory. This file never touches a
// std::vector or a data file directly, it only calls through Inventory,
// which validates input and persists every mutation via Persistence.
//
// products.dat, suppliers.dat, and transactions.dat are read from and
// written to whatever directory the program is run from.

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Inventory.h"

namespace {

// Prints the prompt and returns whatever the user types (exits on end of input).
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\nEnd of input, exiting.\n";
        std::exit(0);
    }
    return line;
}

// Keeps asking until the user types a whole number, then returns it.
int readInt(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt);
        try {
            std::size_t pos = 0;
            int value = std::stoi(line, &pos);
            if (pos == line.size()) {
                return value;
            }
        } catch (const std::exception&) {
        }
        std::cout << "Please enter a whole number.\n";
    }
}

// Keeps asking until the user types a number, then returns it.
double readDouble(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt);
        try {
            std::size_t pos = 0;
            double value = std::stod(line, &pos);
            if (pos == line.size()) {
                return value;
            }
        } catch (const std::exception&) {
        }
        std::cout << "Please enter a number.\n";
    }
}

// Prints the product list as a table, or "(no products)" if it is empty.
void printProducts(const std::vector<Product>& products) {
    if (products.empty()) {
        std::cout << "(no products)\n";
        return;
    }
    std::cout << std::left << std::setw(10) << "SKU" << std::setw(20) << "Name"
               << std::setw(14) << "Category" << std::right << std::setw(10) << "Price"
               << std::setw(6) << "Qty" << std::setw(10) << "Reorder" << "  Supplier\n";
    for (const auto& p : products) {
        std::cout << std::left << std::setw(10) << p.sku << std::setw(20) << p.name
                   << std::setw(14) << p.category << std::right << std::setw(10)
                   << std::fixed << std::setprecision(2) << p.unitPrice << std::setw(6)
                   << p.quantityOnHand << std::setw(10) << p.reorderThreshold << "  "
                   << (p.supplierId.empty() ? "-" : p.supplierId) << "\n";
    }
}

// Walks the user through entering a new product and adds it.
void handleAddProduct(Inventory& inv) {
    Product p;
    p.sku = readLine("SKU: ");
    p.name = readLine("Name: ");
    p.category = readLine("Category: ");
    p.unitPrice = readDouble("Unit price: ");
    p.quantityOnHand = readInt("Quantity on hand: ");
    p.reorderThreshold = readInt("Reorder threshold: ");
    p.supplierId = readLine("Supplier ID (blank if none): ");
    p.imagePath = readLine("Image path (blank if none): ");
    try {
        inv.addProduct(p);
        std::cout << "Added " << p.sku << ".\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Asks for a SKU, lets the user change each field (blank keeps the old value), and saves.
void handleEditProduct(Inventory& inv) {
    std::string sku = readLine("SKU to edit: ");
    const Product* existing = inv.findProduct(sku);
    if (existing == nullptr) {
        std::cout << "No product with SKU " << sku << ".\n";
        return;
    }
    Product updated = *existing;
    std::cout << "Leave a field blank to keep its current value.\n";
    try {
        std::string name = readLine("Name [" + updated.name + "]: ");
        if (!name.empty()) updated.name = name;
        std::string category = readLine("Category [" + updated.category + "]: ");
        if (!category.empty()) updated.category = category;
        std::string price = readLine("Unit price [" + std::to_string(updated.unitPrice) + "]: ");
        if (!price.empty()) updated.unitPrice = std::stod(price);
        std::string qty = readLine("Quantity on hand [" + std::to_string(updated.quantityOnHand) + "]: ");
        if (!qty.empty()) updated.quantityOnHand = std::stoi(qty);
        std::string reorder = readLine("Reorder threshold [" + std::to_string(updated.reorderThreshold) + "]: ");
        if (!reorder.empty()) updated.reorderThreshold = std::stoi(reorder);
        std::string supplierId = readLine("Supplier ID [" + updated.supplierId + "]: ");
        if (!supplierId.empty()) updated.supplierId = supplierId;
        std::string imagePath = readLine("Image path [" + updated.imagePath + "]: ");
        if (!imagePath.empty()) updated.imagePath = imagePath;

        inv.editProduct(sku, updated);
        std::cout << "Updated " << sku << ".\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Asks for a SKU and removes that product from the inventory.
void handleRemoveProduct(Inventory& inv) {
    std::string sku = readLine("SKU to remove: ");
    try {
        inv.removeProduct(sku);
        std::cout << "Removed " << sku << ".\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Lists all products, or only the ones matching a search word.
void handleListOrSearch(const Inventory& inv) {
    std::string query = readLine("Search text (blank to list all): ");
    if (query.empty()) {
        printProducts(inv.products());
    } else {
        printProducts(inv.search(query));
    }
}

// Asks what to sort by and in which order, then prints the sorted list.
void handleSort(const Inventory& inv) {
    std::cout << "Sort by: 1) Name  2) Category  3) Unit price  4) Quantity on hand  5) Reorder threshold\n";
    int choice = readInt("Choice: ");
    Inventory::SortField field;
    switch (choice) {
        case 1: field = Inventory::SortField::Name; break;
        case 2: field = Inventory::SortField::Category; break;
        case 3: field = Inventory::SortField::UnitPrice; break;
        case 4: field = Inventory::SortField::QuantityOnHand; break;
        case 5: field = Inventory::SortField::ReorderThreshold; break;
        default:
            std::cout << "Not a valid choice.\n";
            return;
    }
    std::string order = readLine("Ascending or descending? (a/d, default a): ");
    bool ascending = order.empty() || order[0] == 'a' || order[0] == 'A';
    printProducts(inv.sortedProducts(field, ascending));
}

// Walks the user through entering a new supplier and adds it.
void handleAddSupplier(Inventory& inv) {
    Supplier s;
    s.id = readLine("Supplier ID: ");
    s.name = readLine("Name: ");
    s.contact = readLine("Contact: ");
    try {
        inv.addSupplier(s);
        std::cout << "Added supplier " << s.id << ".\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Prints every supplier with its contact details.
void handleListSuppliers(const Inventory& inv) {
    const auto& suppliers = inv.suppliers();
    if (suppliers.empty()) {
        std::cout << "(no suppliers)\n";
        return;
    }
    for (const auto& s : suppliers) {
        std::cout << s.id << "  " << s.name << "  " << s.contact << "\n";
    }
}

// Asks for a supplier ID and removes it, if no product uses it.
void handleRemoveSupplier(Inventory& inv) {
    std::string id = readLine("Supplier ID to remove: ");
    try {
        inv.removeSupplier(id);
        std::cout << "Removed supplier " << id << ".\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Asks for the details of a stock movement and records it.
void handleRecordMovement(Inventory& inv) {
    std::string sku = readLine("Product SKU: ");
    std::cout << "Type: 1) Stock in  2) Stock out  3) Adjustment (positive correction)\n";
    int choice = readInt("Choice: ");
    TransactionType type;
    switch (choice) {
        case 1: type = TransactionType::StockIn; break;
        case 2: type = TransactionType::StockOut; break;
        case 3: type = TransactionType::Adjustment; break;
        default:
            std::cout << "Not a valid choice.\n";
            return;
    }
    int quantity = readInt("Quantity: ");
    std::string note = readLine("Note (optional): ");
    try {
        Transaction t = inv.recordMovement(sku, type, quantity, note);
        std::cout << "Recorded " << toString(t.type) << " of " << quantity << " for " << sku
                   << " (delta " << t.quantityDelta << ").\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// Prints the movement log, either all of it or just one product's.
void handleTransactionHistory(const Inventory& inv) {
    std::string sku = readLine("Filter by SKU (blank for all): ");
    for (const auto& t : inv.transactions()) {
        if (!sku.empty() && t.productSku != sku) {
            continue;
        }
        std::cout << t.timestamp << "  " << t.productSku << "  " << toString(t.type) << "  "
                   << (t.quantityDelta >= 0 ? "+" : "") << t.quantityDelta << "  " << t.note << "\n";
    }
}

// Prints either the full stock list or just the items that need reordering.
void handleReports(const Inventory& inv) {
    std::cout << "Reports: 1) Current stock  2) Low stock\n";
    int choice = readInt("Choice: ");
    if (choice == 1) {
        printProducts(inv.products());
    } else if (choice == 2) {
        std::vector<Product> low;
        for (const auto& p : inv.products()) {
            if (p.quantityOnHand <= p.reorderThreshold) {
                low.push_back(p);
            }
        }
        printProducts(low);
    } else {
        std::cout << "Not a valid choice.\n";
    }
}

}  // namespace

// Starts the text menu and keeps handling choices until the user quits.
int main() {
    Inventory inventory("products.dat", "suppliers.dat", "transactions.dat");

    std::cout << "Smart Inventory Management System\n";
    std::cout << "Data files are read from and saved to the current directory.\n";

    while (true) {
        std::cout << "\n"
                     "1) Add product\n"
                     "2) Edit product\n"
                     "3) Remove product\n"
                     "4) List / search products\n"
                     "5) Sort products\n"
                     "6) Add supplier\n"
                     "7) List suppliers\n"
                     "8) Remove supplier\n"
                     "9) Record stock movement\n"
                     "10) Transaction history\n"
                     "11) Reports\n"
                     "0) Exit\n";
        int choice = readInt("Choice: ");
        switch (choice) {
            case 1: handleAddProduct(inventory); break;
            case 2: handleEditProduct(inventory); break;
            case 3: handleRemoveProduct(inventory); break;
            case 4: handleListOrSearch(inventory); break;
            case 5: handleSort(inventory); break;
            case 6: handleAddSupplier(inventory); break;
            case 7: handleListSuppliers(inventory); break;
            case 8: handleRemoveSupplier(inventory); break;
            case 9: handleRecordMovement(inventory); break;
            case 10: handleTransactionHistory(inventory); break;
            case 11: handleReports(inventory); break;
            case 0:
                std::cout << "Goodbye.\n";
                return 0;
            default:
                std::cout << "Not a valid choice.\n";
        }
    }
}
