# HostelCare C++ API

See the project root `README.md` for full instructions.

Quick start (WSL Ubuntu, from this folder):
```bash
cmake -S . -B ~/hc-build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/hc-build -j4
~/hc-build/hostelcare_tests                       # 105 checks on anonymised fixtures
mkdir -p ~/hostelcare-data
HOSTELCARE_DATA_DIR=~/hostelcare-data HOSTELCARE_IMPORT_DIR=data ~/hc-build/hostelcare_api
```
