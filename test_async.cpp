#include "include/logfunc.h"
#include <iostream>
#include <chrono>
#include <thread>

void test_loginf_try() {
    std::cout << "\n=== Test 1: loginf_try (Non-blocking) ===\n";
    int value = 0;
    
    for (int i = 0; i < 5; ++i) {
        if (loginf_try(value)) {
            std::cout << "✓ Got value: " << value << "\n";
            return;
        }
        std::cout << "Attempt " << (i+1) << ": No input yet, continuing...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "No input received after 5 attempts.\n";
}

void test_loginf_timeout() {
    std::cout << "\n=== Test 2: loginf_timeout (Timeout version) ===\n";
    int value = 0;
    
    if (loginf_timeout(value, std::chrono::seconds(5))) {
        std::cout << "✓ Got value within timeout: " << value << "\n";
    } else {
        std::cout << "✗ Timeout! Using default value.\n";
        value = 999;
    }
}

void test_loginf_async_future() {
    std::cout << "\n=== Test 3: loginf_async with future ===\n";
    
    auto future = loginf_async<int>();
    
    std::cout << "Main thread continues while waiting...\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "Working... " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Check if ready
        if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            int value = future.get();
            std::cout << "✓ Async got value: " << value << "\n";
            return;
        }
    }
    
    std::cout << "Waiting for final result...\n";
    int value = future.get();
    std::cout << "✓ Final value: " << value << "\n";
}

void test_loginf_async_callback() {
    std::cout << "\n=== Test 4: loginf_async with callback ===\n";
    
    bool done = false;
    
    loginf_async<int>([&done](int value) {
        std::cout << "✓ Callback received value: " << value << "\n";
        done = true;
    });
    
    std::cout << "Main thread continues...\n";
    for (int i = 0; i < 20 && !done; ++i) {
        std::cout << "Main thread work " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Give callback thread time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void test_game_loop_simulation() {
    std::cout << "\n=== Test 5: Game Loop Simulation ===\n";
    std::cout << "Simulating a game loop that checks for input without blocking\n";
    
    int player_input = 0;
    bool input_received = false;
    int frame = 0;
    
    while (frame < 100 && !input_received) { // Max 100 frames
        // Simulate 60 FPS game loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        
        // Check for input (non-blocking)
        if (loginf_try(player_input)) {
            std::cout << "✓ Frame " << frame << ": Player input received: " << player_input << "\n";
            input_received = true;
            break;
        }
        
        // Simulate game update and render
        if (frame % 60 == 0) {
            std::cout << "Frame " << frame << ": Game running... (waiting for input)\n";
        }
        
        frame++;
    }
    
    if (!input_received) {
        std::cout << "No input after " << frame << " frames (timeout)\n";
    }
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "   Non-Blocking loginf Test Suite\n";
    std::cout << "===========================================\n";
    std::cout << "\nPrepare 'in.txt' with a value for testing!\n";
    std::cout << "You can update it during the tests.\n";
    
    int choice = 0;
    std::cout << "\nSelect test:\n";
    std::cout << "1. loginf_try (instant check)\n";
    std::cout << "2. loginf_timeout (5 second timeout)\n";
    std::cout << "3. loginf_async with future\n";
    std::cout << "4. loginf_async with callback\n";
    std::cout << "5. Game loop simulation\n";
    std::cout << "6. Run all tests\n";
    std::cout << "\nEnter choice: ";
    std::cin >> choice;
    
    switch (choice) {
        case 1: test_loginf_try(); break;
        case 2: test_loginf_timeout(); break;
        case 3: test_loginf_async_future(); break;
        case 4: test_loginf_async_callback(); break;
        case 5: test_game_loop_simulation(); break;
        case 6:
            test_loginf_try();
            test_loginf_timeout();
            test_loginf_async_future();
            test_loginf_async_callback();
            test_game_loop_simulation();
            break;
        default:
            std::cout << "Invalid choice\n";
    }
    
    std::cout << "\n===========================================\n";
    std::cout << "   Tests Complete\n";
    std::cout << "===========================================\n";
    
    return 0;
}
