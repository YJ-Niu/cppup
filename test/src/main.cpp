import std;
import app.math;
import app.service;

int main() {
    const int total = app::calculate_total(2, 3);
    std::println("2 + 3 = {}", total);
    std::println("C++23 modules are working");
    return 0;
}
