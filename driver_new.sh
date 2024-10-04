#!/bin/bash
#
# driver.sh - This is a simple autograder for the Proxy Lab. It does
#     basic sanity checks that determine whether or not the code
#     behaves like a concurrent caching proxy. 
#
#     David O'Hallaron, Carnegie Mellon University
#     updated: 2/8/2016
# 
#     usage: ./driver.sh
# 

# Point values
MAX_BASIC=40
MAX_CONCURRENCY=15
MAX_CACHE=45

# Various constants
HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10

# List of text and binary files for the basic test
BASIC_LIST="home.html
            csapp.c
            tiny.c
            godzilla.jpg
            tiny"

CACHE_LIST="tiny.c
home.html
csapp.c
tiny
godzilla.jpg
test1.txt"

# should be cached 6-15
CACHE_LIST1="cache_test/test1.txt
cache_test/test2.txt
cache_test/test3.txt
cache_test/test4.txt
cache_test/test5.txt
cache_test/test6.txt
cache_test/test7.txt
cache_test/test8.txt
cache_test/test9.txt
cache_test/test10.txt
cache_test/test11.txt
cache_test/test12.txt
cache_test/test13.txt
cache_test/test14.txt
cache_test/test15.txt"

CACHE_LIST2="cache_test/test5.txt
cache_test/test4.txt
cache_test/test3.txt
cache_test/test2.txt
cache_test/test1.txt"

# The file we will fetch for various tests

FETCH_LIST1="cache_test/test1.txt
cache_test/test2.txt
cache_test/test3.txt
cache_test/test4.txt
cache_test/test5.txt"

FETCH_LIST2="cache_test/test15.txt
cache_test/test14.txt
cache_test/test13.txt
cache_test/test12.txt
cache_test/test11.txt
cache_test/test10.txt
cache_test/test9.txt
cache_test/test8.txt
cache_test/test7.txt
cache_test/test6.txt"

FETCH_LIST3="cache_test/test1.txt
cache_test/test2.txt
cache_test/test3.txt
cache_test/test4.txt
cache_test/test5.txt
cache_test/test6.txt
cache_test/test7.txt
cache_test/test8.txt
cache_test/test9.txt
cache_test/test10.txt"

FETCH_LIST4="cache_test/test11.txt
cache_test/test12.txt
cache_test/test13.txt
cache_test/test14.txt
cache_test/test15.txt"

CANTCACHE="cache_test/test16.txt"

#####
# Helper functions
#

#
# download_proxy - download a file from the origin server via the proxy
# usage: download_proxy <testdir> <filename> <origin_url> <proxy_url>
#
function download_proxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# download_noproxy - download a file directly from the origin server
# usage: download_noproxy <testdir> <filename> <origin_url>
#
function download_noproxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --output $2 $3 
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# clear_dirs - Clear the download directories
#
function clear_dirs {
    rm -rf ${PROXY_DIR}/*
    rm -rf ${NOPROXY_DIR}/*
}

function clear_noproxy_dirs {
    rm -rf ${NOPROXY_DIR}/*
}

#
# wait_for_port_use - Spins until the TCP port number passed as an
#     argument is actually being used. Times out after 5 seconds.
#
function wait_for_port_use() {
    timeout_count="0"
    portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
        | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
        | grep -E "[0-9]+" | uniq | tr "\n" " "`

    echo "${portsinuse}" | grep -wq "${1}"
    while [ "$?" != "0" ]
    do
        timeout_count=`expr ${timeout_count} + 1`
        if [ "${timeout_count}" == "${MAX_PORT_TRIES}" ]; then
            kill -ALRM $$
        fi

        sleep 1
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`
        echo "${portsinuse}" | grep -wq "${1}"
    done
}


#
# free_port - returns an available unused TCP port 
#
function free_port {
    # Generate a random port in the range [PORT_START,
    # PORT_START+MAX_RAND]. This is needed to avoid collisions when many
    # students are running the driver on the same machine.
    port=$((( RANDOM % ${MAX_RAND}) + ${PORT_START}))

    while [ TRUE ] 
    do
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`

        echo "${portsinuse}" | grep -wq "${port}"
        if [ "$?" == "0" ]; then
            if [ $port -eq ${PORT_MAX} ]
            then
                echo "-1"
                return
            fi
            port=`expr ${port} + 1`
        else
            echo "${port}"
            return
        fi
    done
}


#######
# Main 
#######

######
# Verify that we have all of the expected files with the right
# permissions
#

# Kill any stray proxies or tiny servers owned by this user
killall -q proxy tiny nop-server.py 2> /dev/null

# Make sure we have a Tiny directory
if [ ! -d ./tiny ]
then 
    echo "Error: ./tiny directory not found."
    exit
fi

# If there is no Tiny executable, then try to build it
if [ ! -x ./tiny/tiny ]
then 
    echo "Building the tiny executable."
    (cd ./tiny; make)
    echo ""
fi

# Make sure we have all the Tiny files we need
if [ ! -x ./tiny/tiny ]
then 
    echo "Error: ./tiny/tiny not found or not an executable file."
    exit
fi
for file in ${BASIC_LIST}
do
    if [ ! -e ./tiny/${file} ]
    then
        echo "Error: ./tiny/${file} not found."
        exit
    fi
done

# Make sure we have an existing executable proxy
if [ ! -x ./proxy ]
then 
    echo "Error: ./proxy not found or not an executable file. Please rebuild your proxy and try again."
    exit
fi

# Make sure we have an existing executable nop-server.py file
if [ ! -x ./nop-server.py ]
then 
    echo "Error: ./nop-server.py not found or not an executable file."
    exit
fi

# Create the test directories if needed
if [ ! -d ${PROXY_DIR} ]
then
    mkdir ${PROXY_DIR}
fi

if [ ! -d ${NOPROXY_DIR} ]
then
    mkdir ${NOPROXY_DIR}
fi

# Add a handler to generate a meaningful timeout message
trap 'echo "Timeout waiting for the server to grab the port reserved for it"; kill $$' ALRM

#####
# Basic
#
# echo "*** Basic ***"

# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on ${tiny_port}"
cd ./tiny
./tiny ${tiny_port}   &> /dev/null  &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on ${proxy_port}"
./proxy ${proxy_port}  &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"


# Now do the test by fetching some text and binary files directly from
# Tiny and via the proxy, and then comparing the results.
numRun=0
numSucceeded=0
for file in ${BASIC_LIST}
do
    numRun=`expr $numRun + 1`
    echo "${numRun}: ${file}"
    clear_dirs

    # Fetch using the proxy
    echo "   Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"

    # Fetch directly from Tiny
    echo "   Fetching ./tiny/${file} into ${NOPROXY_DIR} directly from Tiny"
    download_noproxy $NOPROXY_DIR ${file} "http://localhost:${tiny_port}/${file}"

    # Compare the two files
    echo "   Comparing the two files"
    diff -q ${PROXY_DIR}/${file} ${NOPROXY_DIR}/${file} &> /dev/null
    if [ $? -eq 0 ]; then
        numSucceeded=`expr ${numSucceeded} + 1`
        echo "   Success: Files are identical."
    else
        echo "   Failure: Files differ."
    fi
done

echo "Killing tiny and proxy"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

basicScore=`expr ${MAX_BASIC} \* ${numSucceeded} / ${numRun}`

echo "basicScore: $basicScore/${MAX_BASIC}"


######
# Concurrency
#

echo ""
echo "*** Concurrency ***"

# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> tiny.log &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"

# Run a special blocking nop-server that never responds to requests
nop_port=$(free_port)
echo "Starting the blocking NOP server on port ${nop_port}"
python ./nop-server.py ${nop_port} &> nop-server.log &
nop_pid=$!

# Wait for the nop server to start in earnest
wait_for_port_use "${nop_port}"

# Try to fetch a file from the blocking nop-server using the proxy
clear_dirs
echo "Trying to fetch a file from the blocking nop-server"
download_proxy $PROXY_DIR "nop-file.txt" "http://localhost:${nop_port}/nop-file.txt" "http://localhost:${proxy_port}" &

# Fetch directly from Tiny
echo "Fetching ./tiny/${FETCH_FILE} into ${NOPROXY_DIR} directly from Tiny"
download_noproxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}"

# Fetch using the proxy
echo "Fetching ./tiny/${FETCH_FILE} into ${PROXY_DIR} using the proxy"
download_proxy $PROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"

# See if the proxy fetch succeeded
echo "Checking whether the proxy fetch succeeded"
diff -q ${PROXY_DIR}/${FETCH_FILE} ${NOPROXY_DIR}/${FETCH_FILE} &> /dev/null
if [ $? -eq 0 ]; then
    concurrencyScore=${MAX_CONCURRENCY}
    echo "Success: Was able to fetch tiny/${FETCH_FILE} from the proxy."
else
    concurrencyScore=0
    echo "Failure: Was not able to fetch tiny/${FETCH_FILE} from the proxy."
fi

# Clean up
echo "Killing tiny, proxy, and nop-server"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null
kill $nop_pid 2> /dev/null
wait $nop_pid 2> /dev/null

echo "concurrencyScore: $concurrencyScore/${MAX_CONCURRENCY}"

#####
# Caching
#
echo ""
echo "*** Cache ***"

# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"

# Fetch some files from tiny using the proxy
clear_dirs
for file in ${CACHE_LIST1}
do
    echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    FILE_NAME=$(basename "$file")
    download_proxy $PROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done

# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null

cacheScore=0

# Test 1. Max_cache_test. 15 text files have 97KB data each. Cache should have 6-15. Test if correct files are in the cache.
#1-1 search 1-5 textfile in cache. These files should not be found in cache.
for file in ${FETCH_LIST1}
do
    echo "Fetching a cached copy of ./tiny/${file} into ${NOPROXY_DIR}"
    FILE_NAME=$(basename "$file")
    download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done
# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
for file in ${FETCH_LIST1}; do
    diff -q ./tiny/${file} ${NOPROXY_DIR}/${file} &> ./log
    if [ $? -ne 0 ]; then
        ((cacheScore+=1))
        echo "Success: Was not able to fetch tiny/${file} from the cache."
    else
        echo "Failure: Was able to fetch tiny/${file} from the proxy cache."
    fi  
done
# if cacheScore=5 then success
if [ $cacheScore -eq 5 ]; then
    echo "cache don't have old cache...success"
else
    echo "cache still have old cache...fail"
fi

#1-1 search 15-6 textfile in cache. These files should not be found in cache.

for file in ${FETCH_LIST2}
do
    echo "Fetching a cached copy of ./tiny/${file} into ${NOPROXY_DIR}"
    FILE_NAME=$(basename "$file")
    download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}" &> ./logdown
done
# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
for file in ${FETCH_LIST2}
do
    FILE_NAME=$(basename "$file")
    diff -q ./tiny/${file} ${NOPROXY_DIR}/${FILE_NAME}  &> ./log
    if [ $? -eq 0 ]; then
        ((cacheScore+=1))
        echo "Success: Was able to fetch tiny/${file} from the cache."
    else        
        echo "Failure: Was not able to fetch tiny/${file} from the proxy cache."
    fi
done
#if cacheScore=15 then success
if [ $cacheScore -eq 15 ]; then
    echo "cache 1MB test...success"
else
    echo "cache still have old cache or can't find cache data...fail"
fi

# Test 2. LRU_test. Now Cache has 6 to 15. Restart tiny and get 1-5. So cache should have 1-10. Test if correct files are in the cache.
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}
wait_for_port_use "${tiny_port}"
clear_dirs
## get 1-5 from tiny
for file in ${CACHE_LIST2}
do
    echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    FILE_NAME=$(basename "$file")
    download_proxy $PROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done
# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null

## get 1-10 from proxy. These files are in cache so you can get it from proxy.
for file in ${FETCH_LIST3}
do
    echo "Fetching a cached copy of ./tiny/${file} into ${NOPROXY_DIR}"
    FILE_NAME=$(basename "$file")
    download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}" &> ./logdown
done
for file in ${FETCH_LIST3}
do
    FILE_NAME=$(basename "$file")
    diff -q ./tiny/${file} ${NOPROXY_DIR}/${FILE_NAME}  &> ./log
    if [ $? -eq 0 ]; then
        ((cacheScore+=1))
        echo "Success: Was able to fetch tiny/${file} from the cache."
    else        
        echo "Failure: Was not able to fetch tiny/${file} from the proxy cache."
    fi
done
# if cacheScore=10 then success
if [ $cacheScore -eq 25 ]; then
    echo "cached appropriate files...success"
else
    echo "cached wrong files...fail"
fi
## get 11-15 from proxy. These files are not in cache so you can't get it from proxy.
for file in ${FETCH_LIST4}
do
    echo "Fetching a cached copy of ./tiny/${file} into ${NOPROXY_DIR}"
    FILE_NAME=$(basename "$file")
    download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}" &> ./logdown
done
for file in ${FETCH_LIST4}
do
    FILE_NAME=$(basename "$file")
    diff -q ./tiny/${file} ${NOPROXY_DIR}/${FILE_NAME}  &> ./log
    if [ $? -ne 0 ]; then
        ((cacheScore+=1))
        echo "Success: Was not able to fetch tiny/${file} from the cache."
    else        
        echo "Failure: Was able to fetch tiny/${file} from the proxy cache."
    fi
done

# if cacheScore=30 then success
if [ $cacheScore -eq 30 ]; then
    echo "cached appropriate files...success"
else
    echo "cached wrong files...fail"
fi

# 3. check if your cache can block over 100KB file
# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

clear_dirs
FILE_NAME=$(basename "$CANTCACHE")
echo "Fetching ./tiny/${CANTCACHE} into ${PROXY_DIR} using the proxy"
download_proxy $PROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${CANTCACHE}" "http://localhost:${proxy_port}"

# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null

# Try fetch test16.txt which is over 100KB
echo "Fetching a cached copy of ./tiny/${CANTCACHE} into ${NOPROXY_DIR}"
download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${CANTCACHE}" "http://localhost:${proxy_port}"
echo "./tiny/${CANTCACHE} ${NOPROXY_DIR}/${FILE_NAME}"
diff -q ./tiny/${CANTCACHE} ${NOPROXY_DIR}/${FILE_NAME}

if [ $? -ne 0 ]; then
    ((cacheScore+=5))
    echo "Success: Was not able to fetch tiny/${CANTCACHE} from the cache."
else
    echo "Failure: Was able to fetch tiny/${CANTCACHE} from the proxy cache."
fi

if [ $cacheScore -eq 35 ]; then
    echo "can't cache over 100kB...success"
else
    echo "cached wrong files...fail"
fi

## get 1-10 from proxy. These files are in cache so you can get it from proxy.
for file in ${FETCH_LIST3}
do
    echo "Fetching a cached copy of ./tiny/${file} into ${NOPROXY_DIR}"
    FILE_NAME=$(basename "$file")
    download_proxy $NOPROXY_DIR ${FILE_NAME} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}" &> ./logdown
done
for file in ${FETCH_LIST3}
do
    FILE_NAME=$(basename "$file")
    diff -q ./tiny/${file} ${NOPROXY_DIR}/${FILE_NAME}  &> ./log
    if [ $? -eq 0 ]; then
        ((cacheScore+=1))
        echo "Success: Was able to fetch tiny/${file} from the cache."
    else
             
        echo "Failure: Was not able to fetch tiny/${file} from the proxy cache."
    fi
done
if [ $cacheScore -eq 45 ]; then
    echo "cached appropriate files...success"
else
    echo "cached wrong files...fail"
fi

#Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "cacheScore: $cacheScore/${MAX_CACHE}"

# Emit the total score
totalScore=`expr ${basicScore} + ${cacheScore} + ${concurrencyScore}`
maxScore=`expr ${MAX_BASIC} + ${MAX_CACHE} + ${MAX_CONCURRENCY}`
echo ""
echo "totalScore: ${totalScore}/${maxScore}"
exit

