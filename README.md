# yandex_cpp_black_belt

This repo contains the final version of notorious Transport Catalog task from C++ Black Belt course by Yandex (which I really had fun with in 2021).

The tool takes transport database description (buses, stops, places of interest) in JSON format (<build_dir>/main make_base <input.json>) and processes queries (<build_dir>/main process_queries <stat.json>): provides info about buses, stops, shortest routes... see "stat" JSON files in "tests" for details, unfortunately names are in Russian.

Disclaimer: all code is mine except most part of graph.h (provided by course authors).

## Requirements

cmake 3.10+
protobuf
compiler with c++17 support

## Build and run

Tested on Ubuntu 20.04

```
cmake <repo_root>/transport_catalog
make -j
<repo_root>/transport_catalog/run_tests.sh
```
