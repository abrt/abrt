function syslog() {
    local msg="${1}"
    logger -t abrt-testsuite -- "-------------------------------------"
    logger -t abrt-testsuite -- "MARK: $msg"
    logger -t abrt-testsuite -- "-------------------------------------"
}

function run_stage() {
    local msg="Running stage: ${1}"
    local endmsg="End of stage: ${1}"
    local dirname=$( echo ${1} | tr "[:upper:]" "[:lower:]" )
    local varname="${1}_SCRIPT"
    local script="${!varname}"
    local dir="$OUTPUT_ROOT/$dirname"

    syslog "$msg"
    mkdir -p "$dir"
    if [ ${1} = "TEST" ]; then
        echo_success
        echo " $msg"
        $script | tee "$dir/stage.log"
        if [ $? != 0 ]; then
            touch "$dir/failed"
        fi
        sed -r -i "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g" "$dir/stage.log"
    else
        $script &> "$dir/stage.log"
        if [ $? != 0 ]; then
            echo_failure
            touch "$dir/failed"
        else
            echo_success
        fi
        echo " $msg"
    fi
    syslog "$endmsg"

    # collect /var/log/messages for the stage
    start=$( grep -n "MARK: Running.*: ${1}" '/var/log/messages'  | tail -n 1 | awk -F: '{print $1}' )
    end=$( grep -n "MARK: End.*: ${1}" '/var/log/messages'  | tail -n 1 | awk -F: '{print $1}' )
    start=$[ $start + 2 ]
    end=$[ $end - 2 ]

    sed -n "${start},${end}p;${end}q" '/var/log/messages' > "$dir/messages"

    # collect dmesg for the stage
    dmesg -c > "$dir/dmesg"
}

# colorful OK/FAIL formatting

export SETCOLOR_SUCCESS="echo -en \\033[1;32m"
export SETCOLOR_FAILURE="echo -en \\033[1;31m"
export SETCOLOR_NORMAL="echo -en \\033[0;39m"

# Function to print the SUCCESS status
echo_success() {
  echo -n "["
  $SETCOLOR_SUCCESS
  echo -n $"  OK  "
  $SETCOLOR_NORMAL
  echo -n "]"
  return 0
}

# Function to print the FAILED status message
echo_failure() {
  echo -n "["
  $SETCOLOR_FAILURE
  echo -n $"FAILED"
  $SETCOLOR_NORMAL
  echo -n "]"
  return 1
}
