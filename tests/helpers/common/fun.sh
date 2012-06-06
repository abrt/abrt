#!/bin/bash

function reinit_vm() {
    if [ -z "${VM_NAME+x}" ]; then
        echo '$VM_NAME variable missing'
        return
    fi

    if virsh --connect qemu:///system list | grep -q $VM_NAME;  then
        echo 'Destroying virtual machine'
        virsh --connect qemu:///system destroy $VM_NAME
        sleep 3
    fi

    if virsh --connect qemu:///system list --all | grep -q $VM_NAME;  then
        echo 'Undef virtual machine'
        virsh --connect qemu:///system undefine $VM_NAME
        sleep 1
    fi
}

function get_ks() {
    if [ -z "${REPO+x}" ]; then
        echo '$REPO variable missing'
        return
    fi

    if [ -z "${KS_NAME_PREFIX+x}" ]; then
        echo '$KS_NAME_PREFIX variable missing'
        return
    fi

    git clone $REPO git_dir
    origks=$( find . -name "${KS_NAME_PREFIX}.kickstart.cfg" )
    pushd $( dirname $origks )
    ksflatten $( basename $origks ) > orig-ks.cfg
    popd
    mv $( dirname $origks )/orig-ks.cfg .
    rm -rf git_dir
}

function add_rhel_repos() {
    # add rhel repos if required
    if [ -n "${REPOS_REQUIRED+x}" ]; then
        git clone $REPO git_dir
        origks=$( find . -name "${KS_NAME_PREFIX}.kickstart.cfg" )
        path=$( dirname $origks )/repos

        for rep in $path/${REPOS_REQUIRED}/*; do
            embed_file "/etc/yum.repos.d/$( basename $rep)" $rep
        done

        # add brq mock config
        embed_file $( cat $path/${REPOS_REQUIRED}/mock_target ) $path/${REPOS_REQUIRED}/mock.cfg
        rm -rf git_dir
    fi
}
function add_abrt_repos() {
    # add abrt nightly repo
    git clone $REPO git_dir
    origks=$( find . -name "${KS_NAME_PREFIX}.kickstart.cfg" )
    path=$( dirname $origks )/repos
    if [ -n "${REPOS_REQUIRED+x}" ]; then
        # use epel repo
        embed_file "/etc/yum.repos.d/epel-abrt.repo" $path/epel-abrt.repo
    else
        # use fedora repo
        embed_file "/etc/yum.repos.d/fedora-abrt.repo" $path/fedora-abrt.repo
    fi
    rm -rf git_dir
}

function embed_file() {
    local target_path="${1}"
    local local_path="${2}"
    echo "Embedding $local_path with target: $target_path"

    cat >> custom-ks.cfg << _EE_
cat > $target_path << _END_
_EE_
    cat $local_path >> custom-ks.cfg
    cat >> custom-ks.cfg << _EE_
_END_
_EE_
}

function handle_results() {
    local result_dir="${1}"
    local target_dir="${2}"
    local current="$(date +%F)"
    local suffix="$(date +%R)"

    pushd $target_dir
        tar xzvf ${result_dir}/${current}.tar.gz
        echo mv abrt-testsuite "${current}_${suffix}"
        mv abrt-testsuite "${current}_${suffix}"
    popd

    result_path="${target_dir}/${current}_${suffix}"

    rm ${result_dir}/${current}.tar.gz
}
function res_mail() {
    local res_path=${1}
    pushd $res_path
    touch mail.summary
        result='PASS'
    date="$(date '+%F %R')"
    partial=$( echo $res_path | sed 's#/var/nightly_results/##' )
    platform=$( dirname $partial )
    date=$( basename $partial )
    dom='http://rmarko.fedorapeople.org/abrt/'

    if grep -q 'FAIL' 'results'; then
        result='FAIL'
        echo 'Failed tests:' >> 'mail.summary'
        for ft in $( find . -type f -name 'fail.log' ); do
        dir=$( dirname $ft )
        ts=$( basename $dir )

        echo " - $ts:" >> 'mail.summary'
        cat "$dir/fail.log" >> 'mail.summary'
        # build web link

        link="${dom}${partial}/index.html#${date}_${ts}_fail"
        echo "Partial: $partial"
        echo "Date: $date"
        echo "Dom: $dom"
        echo "Platform: $platform"
        echo "Link: $link"
        echo " - $link" >> 'mail.summary'
        echo "" >> 'mail.summary'
        done
    else
        echo 'All tests passed' >> 'mail.summary'
    fi

    if cat mail.summary | mail -v -s "[$date] [$result] [$platform] ABRT testsuite report" -r $MAILFROM $MAILTO; then
        echo "Mail: OK"
    else
        echo "Mail: FAILED"
    fi
    popd
}
