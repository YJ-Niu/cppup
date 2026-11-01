export module app.service;

import app.math;

export namespace app {
    constexpr int calculate_total(int a, int b) noexcept {
        return add(a, b);
    }
}
