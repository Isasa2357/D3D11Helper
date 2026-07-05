#include "TestCommon.hpp"

int main(int argc, char** argv) {
    const std::string suite = (argc >= 2) ? argv[1] : std::string();
    return Test::Run(suite);
}
