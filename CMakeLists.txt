cmake_minimum_required(VERSION 3.20)

project(matching-engine VERSION 0.1.0 LANGUAGES CXX)
add_compile_definitions(NDEBUG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_library(orderbook 
    # util
    src/util/timestamp.cpp
    src/util/i_clock.cpp
    src/util/system_clock.cpp
    src/util/simulated_clock.cpp
    src/util/id_generator.cpp

    # core
    src/core/order.cpp
    src/core/trade.cpp
    src/core/price_level.cpp
    src/core/order_book_side.cpp
    src/core/order_book.cpp
    src/core/matching_engine.cpp

    # report
    src/report/report_service.cpp
    src/report/volume_report.cpp
    src/report/price_report.cpp
    src/report/internal_trade_repository.cpp
)

target_include_directories(orderbook
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(orderbook PUBLIC cxx_std_20)

add_executable(web_demo 
    src/examples/web_demo/main.cpp
)
target_link_libraries(web_demo PRIVATE orderbook)


target_include_directories(web_demo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(web_demo PRIVATE
  ASIO_STANDALONE
  _WIN32_WINNT=0x0601
)

if(WIN32)
  target_link_libraries(web_demo PRIVATE ws2_32 mswsock wsock32)
endif()

add_executable(multithread_test
    src/examples/multithread_test.cpp
)
target_link_libraries(multithread_test PRIVATE orderbook)
target_include_directories(multithread_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)