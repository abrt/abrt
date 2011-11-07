#!/bin/bash
# Install Fedora 15 virtual machine and run the tests

DIR="$( cd "$( dirname "$0" )" && pwd )"

echo "*** $0 started"

trap "echo; echo '*** TIMEOUT ***'; exit 255" SIGUSR1

( sleep 1h; kill -SIGUSR1 $$ > /dev/null 2>&1 ) &
tstamp="$(date '+%F %X' | sed 's/ /_/g' | sed 's/:/./g')"
VM="Fedora_TESTBOT_${tstamp}"

# Enable in production
#trap "if virsh list | grep -q '$VM'; then echo 'Destroy VM: $VM'; virsh destroy $VM; fi; echo 'Delete VM: $VM'; virsh undefine $VM; echo '*** $0 finished'" EXIT

echo "Install VM: $VM"
virt-install --name "$VM" --ram "1222" \
    --connect qemu:///system \
    --location "http://download.fedoraproject.org/pub/fedora/linux/releases/15/Fedora/x86_64/os/" \
    --disk path=/var/lib/libvirt/images/$VM.img,size=4,sparse=true \
    --accelerate \
    --initrd-inject=$DIR/anaconda-ks.cfg \
    --extra-args "ks=file:/anaconda-ks.cfg" \
    --graphics type=vnc \
    --noautoconsole

echo -n "Zzzz (until VM is being installed): "
while virsh list | grep -q "$VM"; do
    echo -n '.'
    sleep 1m
done
echo 'x'

if virsh list --all | grep -q "$VM.*shut off"; then
    echo "Start VM: $VM"
    virsh start $VM
fi

echo -n "Zzzz (until VM is executing the tests): "
while virsh list | grep -q "$VM"; do
    echo -n '.'
    sleep 1m
done
echo 'x'

exit 0
