#!/bin/sh

URL=$1
COUNT=$2

start_time=$(date +%s.%N)

i=1
while [ "$i" -le "$COUNT" ]; do
    echo "Request #$i to $URL"
    curl "$URL" > /dev/null 2>&1 &
    i=$((i + 1))
done

wait

end_time=$(date +%s.%N)

elapsed=$(echo "$end_time - $start_time" | bc)

echo "Total elapsed time: $elapsed seconds"
