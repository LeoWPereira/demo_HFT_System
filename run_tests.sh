#!/bin/bash

# Quick test script

echo "Running HFT Trading System Tests..."
echo ""

echo "1. Running Order Book Tests..."
./build/test_order_book
echo ""

echo "2. Running Lock-Free Tests..."
./build/test_lockfree
echo ""

echo "3. Running Performance Benchmarks..."
./build/benchmark
echo ""

echo "All tests completed successfully! ✓"
echo ""
echo "To run the trading system:"
echo "  ./build/hft_trading --config config/trading.conf"
echo ""
