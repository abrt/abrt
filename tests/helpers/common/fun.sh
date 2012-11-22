#!/bin/bash

function wait_for_vm() {
    echo "Waiting for the machine to boot"

    set +x

    try=0;
    while true; do
      ping -w 1 $IP &> /dev/null;
      res=$?;
      if [ $res == 0 ]; then
        break;
      fi;

      if [ $try == 100 ]; then # max tries
         echo "Seems like machine '$VM_NAME' doesn't want to talk with us on '$IP'"
         exit 1;
      fi
      let try=try+1;
      sleep 1; # wait between the attempts, otherwise it floods the log
    done;

    set -x
}

function reinit_vm() {
    if [ -z "${VM_NAME+x}" ]; then
        echo '$VM_NAME variable missing'
        return
    fi

    if virsh --connect qemu:///system list | grep -q "${VM_NAME}_clone";  then
        echo 'Destroying virtual machine'
        virsh --connect qemu:///system destroy "${VM_NAME}_clone"
    fi

    if virsh --connect qemu:///system list --all | grep -q "${VM_NAME}_clone";  then
        echo 'Undef virtual machine'
        virsh --connect qemu:///system undefine "${VM_NAME}_clone"
    fi

    if test -e "${DISK}_clone"; then
        echo 'Removing current snapshot'
        sudo lvremove -f "${DISK}_clone"
    fi

    echo "Creating snapshot of ${VM_NAME}"
    orig_device="$( echo $DISK | sed 's#mapper/##g;s#vol0-#vol0/#' )"
    sudo lvcreate -L 1G -s -n "$( basename "${DISK}_clone" | sed 's#vol0-##')" "$orig_device"

    virt-clone --connect qemu:///system -o "${VM_NAME}" -n "${VM_NAME}_clone" \
        -f "${DISK}_clone" --preserve-data -m ${MAC}

    if [ $? -ne 0 ]; then
        echo "Restoring VM failed";
        exit 1;
    else
        echo "VM restored";
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
    if [ ! -n "${REPOS_REQUIRED+x}" ]; then
        # add fedora repo
        embed_file "/etc/yum.repos.d/fedora-abrt.repo" $path/fedora-abrt.repo
    fi
    rm -rf git_dir
}

function embed_file() {
    local target_path="${1}"
    local local_path="${2}"
    echo "Embedding $local_path with target: $target_path"

    cat >> custom-ks.cfg << _EE_
cat > $target_path << "_END_"
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

function recreate_vm() {
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

    #reinit_vm
    echo 'Getting kickstart'
    get_ks

    cp orig-ks.cfg custom-ks.cfg
    rm -f orig-ks.cfg

    echo 'Patching kickstart'

    cat ~/common/start_post_hunk >> custom-ks.cfg
    cat ~/common/git_hunk >> custom-ks.cfg

    # ssh key
    cat ~/common/ssh_pre_hunk >> custom-ks.cfg
    embed_file "/root/.ssh/id_rsa" ~/.ssh/id_rsa
    # ssh login key
    embed_file "/root/.ssh/authorized_keys" ~/.ssh/id_rsa.pub
    cat ~/common/ssh_post_hunk >> custom-ks.cfg

    # procmail cfg
    cat ~/common/procmail_hunk >> custom-ks.cfg

    # custom cfg
    embed_file "/root/abrt/tests/runtests/aux/config.sh.local" ./config.sh.local

    # fedorahosted alias
    embed_file "/etc/hosts" ~/common/hosts_hunk
    embed_file "/etc/motd" ~/common/motd_hunk

    # turn off noaudit
    cat ~/common/noaudit_hunk >> custom-ks.cfg

    add_rhel_repos
    add_abrt_repos

    # exclude build tests
    cat ~/common/exclude_build_tests >> custom-ks.cfg

    # disable gpg check
    cat ~/common/no_gpg_check >> custom-ks.cfg

    # %post
    cat ~/common/end_post_hunk >> custom-ks.cfg

    echo 'Running virt-install'
    virt-install --name $VM_NAME --ram "1300" \
      --connect qemu:///system \
      --location "$LOC" \
      --disk path=$DISK,cache=none \
      --initrd-inject=./custom-ks.cfg --extra-args "ks=file:/custom-ks.cfg ${INIT_EXTRA_ARGS}" \
      --noautoconsole \
      --os-type=linux \
      --os-variant=$OS_VARIANT \
      --graphics type=vnc \
      --quiet \
      --network bridge:br0,mac=$MAC

    set +x
    echo 'virt-install done'
    echo -n "Zzzz (until VM is being installed): "
    while virsh --connect qemu:///system list | grep -q $VM_NAME; do
        echo -n '.'
        sleep 1m
    done
    echo 'x'

    virsh --connect qemu:///system start $VM_NAME
    wait_for_vm

    echo "Giving the sshd a while to start"
    sleep 5

    echo "Powering of the machine"

    set +x

    try=0;
    while true; do
      ssh -o StrictHostKeyChecking=no root@$IP "halt -p"
      res=$?;
      if [ $res == 0 ]; then
        break;
      fi;

      if [ $try == 100 ]; then # max tries
         echo "Seems like machine '$VM_NAME' doesn't want to talk with us on '$IP'"
         exit 1;
      fi
      let try=try+1;
      sleep 1; # wait between the attempts, otherwise it floods the log
    done;

    set -x

}
