#include <iostream>

int run_image_pipeline_tests();

int main() {
    const int failures = run_image_pipeline_tests();
    if (failures != 0) {
        std::cerr << failures << " image/pipeline test assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE canonical image/pipeline contracts passed.\n";
    return 0;
}
