#include "render/render.h"
#include <cstdlib>
#include <exception>

int main(int argc, char *argv[]) {
    HelloTriangleApplication app;
    std::cout << argv[0] << std::endl;
    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}