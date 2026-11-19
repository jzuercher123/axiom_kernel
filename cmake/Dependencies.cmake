include(FetchContent)

if(AXIOM_BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.18.0
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(AXIOM_ENABLE_ZMQ)
    find_package(cppzmq CONFIG REQUIRED)
endif()

if(AXIOM_ENABLE_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()