#!/bin/bash

RESULT_FILE="buffer_test_results.csv"
echo "BufferSize(Bytes),Speed" > $RESULT_FILE

BASE_SIZE=4096
MULTIPLES=(1 2 4 8 16 32 64 128)

for m in "${MULTIPLES[@]}"
do
    BUFFER_SIZE=$((BASE_SIZE * m))
    
    echo "Testing buffer size: ${BUFFER_SIZE} bytes..."
    
    output=$(dd if=/dev/zero of=/dev/null bs=$BUFFER_SIZE count=100000 2>&1)
    
    # 改进版：从含 "copied," 的行提取速度
    speed_line=$(echo "$output" | grep -i 'copied,')
    speed=$(echo "$speed_line" | awk '{print $(NF-1)" "$NF}')
    
    echo "$BUFFER_SIZE,$speed" >> $RESULT_FILE
done

echo "Test completed. Results saved to $RESULT_FILE"