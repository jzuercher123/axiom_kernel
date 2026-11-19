include(GoogleTest)

add_executable(axiom_core_tests
        core/test_tensor_view.cpp
        core/test_ring_buffer.cpp
        core/test_pipeline.cpp
)

target_link_libraries(axiom_core_tests
        PRIVATE
        axiom::core
        GTest::gtest_main
)

gtest_discover_tests(axiom_core_tests)

add_executable(axiom_adapter_tests
        adapters/test_bayesian_kl_gate.cpp
)

target_link_libraries(axiom_adapter_tests
        PRIVATE
        axiom::core
        GTest::gtest_main
)

gtest_discover_tests(axiom_adapter_tests)

if(AXIOM_ENABLE_ZMQ)
    add_executable(axiom_zmq_tests
            adapters/test_zmq_publisher.cpp
    )
    target_link_libraries(axiom_zmq_tests
            PRIVATE
            axiom::core
            cppzmq
            GTest::gtest_main
    )
    gtest_discover_tests(axiom_zmq_tests)
endif()

if(AXIOM_ENABLE_OPENSSL)
    add_executable(axiom_crypto_tests
            adapters/test_crypto_provenance.cpp
    )
    target_link_libraries(axiom_crypto_tests
            PRIVATE
            axiom::core
            OpenSSL::Crypto
            GTest::gtest_main
    )
    gtest_discover_tests(axiom_crypto_tests)
endif()