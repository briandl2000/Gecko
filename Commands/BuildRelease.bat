cd ..
mkdir build
cd build
cmake .. -DRELEASE=ON
cmake --build . --config Release

ECHO "Finished building Release"
