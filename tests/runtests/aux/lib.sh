#!/bin/bash

function check_prior_crashes() {
    rlAssert0 "No prior crashes recorded" $(abrt-cli list | wc -l)
    if [ ! "_$(abrt-cli list | wc -l)" == "_0" ]; then
        rlDie "Won't proceed"
    fi
}
