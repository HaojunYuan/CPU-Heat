# Overview
This project implements a timing simulator for an Fire out-of-order, Complete out-of-order (FOCO) CPU using the PREGS/RAT approach. This CPU is equipped with a Branch Predictor, Instruction Cache, Data Cache, and Store Buffer. THe CPU is able to execute Add, Multiply, Load, Store and Branch instructions. In order to enable correct behavior with memory operations in parallel, the CPU uses a memory disambiguation algorithm.

## Run Docker
docker-macos.sh

## Compile code and run validation
make validate
