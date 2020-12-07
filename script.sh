#!/bin/bash

# Expects zip files in a folder ./submissions/
# Expects grader files in ./grading-ex1/


filter_str=$1

# nullglob, so that empty directories will not be iterated
shopt -s nullglob

# Print a header line
echo "Name,1_1_nounmap,1_2_nounmap,1_1_eqloc,1_2_eqloc,1_1,1_2,2_1_noinsf,2_2_noinsf,2_3_noinsf,2_1_nounmap,2_2_nounmap,2_3_nounmap,2_1_eqloc,2_2_eqloc,2_3_eqloc,2_1,2_2,2_3,"

function tester()
{
    return 5
}
function compile_ex1()
{
    rm grader_ex1 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex1.c -lpthread -lrt -o grader_ex1 2>&1) && -f grader_ex1 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex1_eqloc()
{
    rm grader_ex1 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex1_eqloc.c -lpthread -lrt -o grader_ex1 2>&1) && -f grader_ex1 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex1_nounmap()
{
    rm grader_ex1 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex1_nounmap.c -lpthread -lrt -o grader_ex1 2>&1) && -f grader_ex1 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex2()
{
    rm grader_ex2 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex2.c -lpthread -lrt -o grader_ex2 2>&1) && -f grader_ex2 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex2_eqloc()
{
    rm grader_ex2 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex2_eqloc.c -lpthread -lrt -o grader_ex2 2>&1) && -f grader_ex2 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex2_nounmap()
{
    rm grader_ex2 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex2_nounmap.c -lpthread -lrt -o grader_ex2 2>&1) && -f grader_ex2 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function compile_ex3()
{
    rm grader_ex3 2>/dev/null
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c grader_ex3.c -lpthread -lrt -o grader_ex3 2>&1) && -f grader_ex3 ]]
    then
        return 0
    else
        return 99 # error (compilation failed)
    fi
}
function prod_ex()
{
    # Compile the code
    if [[ -z $(gcc -std=c99 -w -g -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE shmheap.c prodder.c -lpthread -lrt -o prodder 2>&1) && -f prodder ]]
    then
        $(timeout --signal=KILL 10s ./prodder prod.txt 1>/dev/null 2>/dev/null)
        args=($(<prod.txt))
        if [[ ${#args[@]} -ne 2 ]]
        then
            return 103
        fi
        first_space=${args[0]}
        mid_space=${args[1]}
        #echo -n "first:$first_space; mid:$mid_space,"
        if [[ $first_space -ge 0 ]] && [[ $first_space -le 80 ]] && [[ $mid_space -ge 0 ]] && [[ $mid_space -le 16 ]]
        then
            if [[ $((first_space % 8)) -eq 0 ]] && [[ $((mid_space % 8)) -eq 0 ]]
            then
                return 0
            else
                return 102
            fi
        else
            return 101
        fi
    else
        return 99 # error (compilation failed)
    fi
}

# Function to do grading for a single exercises
# Returns 0 on success, nonzero on error
function grade_ex1()
{
    test_index=$1
    case $test_index in
        1)
            $(timeout --signal=KILL 10s ./grader_ex1 1 986343578 1>/dev/null 2>/dev/null)
            ;;
        2)
            $(timeout --signal=KILL 10s ./grader_ex1 10 321196728 1>/dev/null 2>/dev/null)
            ;;
        *)
            Message="Invalid test index"
            ;;
    esac
    return $?
}
function grade_ex2()
{
    test_index=$1
    disallow_insufficient_space=$2
    rm test.in 2>/dev/null
    rm test.out 2>/dev/null
    rm sim.out 2>/dev/null
    rm student.out 2>/dev/null
    case $test_index in
        1)
            $(./gen2 $first_space $mid_space 20 496058235 test.in test.out $disallow_insufficient_space)
            ;;
        2)
            $(./gen2 $first_space $mid_space 100 786423160 test.in test.out $disallow_insufficient_space)
            ;;
        3)
            $(./gen2 $first_space $mid_space 1000 1985435896 test.in test.out $disallow_insufficient_space)
            ;;
        *)
            Message="Invalid test index"
            ;;
    esac
    if ! [[ $? -eq 0 ]]
    then
        echo "Ex2 generator or simulator is not working"
        return 100
    fi
    $(./sim2 $first_space $mid_space 999999999 $disallow_insufficient_space < test.in > sim.out)
    if ! [[ $? -eq 0 ]]
    then
        echo "Ex2 generator or simulator is not working"
        return 100
    fi
    if ! cmp -s "test.out" "sim.out"
    then
        echo "Ex2 generator or simulator is not working"
        return 100
    fi
    $(timeout --signal=KILL 10s ./grader_ex2 < test.in > student.out 2>/dev/null)
    ex2_result=$?
    # echo "Res: $ex2_result"
    if ! [[ $ex2_result -eq 0 ]]
    then
        return $ex2_result
    fi
    if cmp -s "test.out" "student.out"
    then
        return 0
    else
        return 1
    fi
}
function grade_ex3()
{
    test_index=$1
    case $test_index in
        1)
            $(timeout --signal=KILL 10s ./grader_ex3 $first_space $mid_space 20 438649583 1>/dev/null 2>/dev/null)
            ;;
        2)
            $(timeout --signal=KILL 10s ./grader_ex3 $first_space $mid_space 100 1320493504 1>/dev/null 2>/dev/null)
            ;;
        3)
            $(timeout --signal=KILL 10s ./grader_ex3 $first_space $mid_space 200 1853546489 1>/dev/null 2>/dev/null)
            ;;
        *)
            Message="Invalid test index"
            ;;
    esac
    return $?
}

# Prep the generator
if ! [[ -z $(g++ -std=c++17 -w -O3 gen-ex2/gen.cpp -o gen2 2>&1) && -f gen2 ]]
then
    echo "Ex2 generator failed to compile"
fi

# Prep the generator
if ! [[ -z $(g++ -std=c++17 -w -O3 dynspace-ex2/simulate.cpp -o sim2 2>&1) && -f sim2 ]]
then
    echo "Ex2 validator failed to compile"
fi

# Loop through all the student submissions
for f in ./submissions/*
do
    # If it is a zip file
    if [[ $f == *.zip && $f == *$filter_str* ]]
    then
        # Echo the student name
        FILE_NAME_ONLY=$(basename "$f")
        STUDENT_NAME=${FILE_NAME_ONLY%% - *}
        echo -n "$STUDENT_NAME,"
        # Create an empty directory for compilation and evaluation
        yes | rm -rf stage 1>/dev/null 2>/dev/null
        mkdir stage
        # Unzip the student directory
        unzip "$f" -d ./stage > /dev/null
        # If the code is in a nested directory, flatten the directory structure
        while true
        do
            for f2 in ./stage/*
            do
                if [[ "$f2" == *"ex4"* ]]
                then
                    # ignore all ex4 code
                    rm -r "$f2" 1>&2
                elif [[ -d $f2 ]]
                then
                    set -- "$f2/"*
                    if [[ $# -gt 0 ]]
                    then
                        mv -v "${f2}/"* ./stage/ 1>&2
                    fi
                    yes | rm -r "$f2" 1>&2
                    continue 2
                fi
            done
            break
        done
        # Remove anything that isn't shmheap.*
        for f2 in ./stage/*
        do
            if [[ $f2 != */shmheap.* ]]
            then
                yes | rm "$f2" 1>&2
            fi
        done
        # Copy the grader files
        cp ./grading-ex1/* ./stage
        cp ./grading-ex2/* ./stage
        cp ./grading-ex3/* ./stage
        cp ./gen2 ./stage
        cp ./sim2 ./stage
        # Go into the stage directory
        cd stage
        
        # Ex1 has 2 tests
        max_test_index=2
        
        # Compile the student's code
        compile_ex1_nounmap
        compile_result=$?
        
        # Test 1
        for ((i=1;i<=max_test_index;i++))
        do
            if [[ $compile_result -eq 0 ]]
            then
                grade_ex1 $i
                RESULT=$?
            else
                RESULT=$compile_result
            fi
            echo -n "$RESULT,"
        done
        
        # Compile the student's code
        compile_ex1_eqloc
        compile_result=$?
        
        # Test 1
        for ((i=1;i<=max_test_index;i++))
        do
            if [[ $compile_result -eq 0 ]]
            then
                grade_ex1 $i
                RESULT=$?
            else
                RESULT=$compile_result
            fi
            echo -n "$RESULT,"
        done
        
        # Compile the student's code
        compile_ex1
        compile_result=$?
        
        # Test 1
        for ((i=1;i<=max_test_index;i++))
        do
            if [[ $compile_result -eq 0 ]]
            then
                grade_ex1 $i
                RESULT=$?
            else
                RESULT=$compile_result
            fi
            echo -n "$RESULT,"
        done
        
        # Prodder (test 2 and 3, sets start_space and mid_space)
        prod_ex
        prod_result=$?
        compile_result=$prod_result
        
        # Ex2 has 3 tests
        max_test_index=3
        
        if [[ $compile_result -eq 0 ]]
        then
            # Compile the student's code
            compile_ex2
            compile_result=$?
        
            # Test 2
            for ((i=1;i<=max_test_index;i++))
            do
                if [[ $compile_result -eq 0 ]]
                then
                    grade_ex2 $i 1
                    RESULT=$?
                else
                    RESULT=$compile_result
                fi
                echo -n "$RESULT,"
            done
            
            # Compile the student's code
            compile_ex2_nounmap
            compile_result=$?
        
            # Test 2
            for ((i=1;i<=max_test_index;i++))
            do
                if [[ $compile_result -eq 0 ]]
                then
                    grade_ex2 $i
                    RESULT=$?
                else
                    RESULT=$compile_result
                fi
                echo -n "$RESULT,"
            done
            
            # Compile the student's code
            compile_ex2_eqloc
            compile_result=$?
        
            # Test 2
            for ((i=1;i<=max_test_index;i++))
            do
                if [[ $compile_result -eq 0 ]]
                then
                    grade_ex2 $i
                    RESULT=$?
                else
                    RESULT=$compile_result
                fi
                echo -n "$RESULT,"
            done
            
            # Compile the student's code
            compile_ex2
            compile_result=$?
        
            # Test 2
            for ((i=1;i<=max_test_index;i++))
            do
                if [[ $compile_result -eq 0 ]]
                then
                    grade_ex2 $i
                    RESULT=$?
                else
                    RESULT=$compile_result
                fi
                echo -n "$RESULT,"
            done
            
            # Compile the student's code
            compile_ex3
            compile_result=$?
        
            # Test 3
            for ((i=1;i<=max_test_index;i++))
            do
                if [[ $compile_result -eq 0 ]]
                then
                    grade_ex3 $i
                    RESULT=$?
                else
                    RESULT=$compile_result
                fi
                echo -n "$RESULT,"
            done
        else
            for ((i=1;i<=max_test_index;i++))
            do
                echo -n "$compile_result,$compile_result,$compile_result,$compile_result,$compile_result,"
            done
        fi
        
        # Go out of the stage directory
        cd ..
        # Remove the stage directory
        yes | rm -rf stage 1>&2
        
        # Print newline
        echo ""
    fi
done
yes | rm -rf ./stage > /dev/null
