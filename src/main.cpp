#include "app.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        rh::App{}.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}