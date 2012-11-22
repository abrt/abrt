#! /bin/bash -x

if [ ! -d abrt ]; then
    git clone git://abrt.brq.redhat.com/abrt.git
fi

pushd /root/abrt/tests/runtests/aux/
ln -s ~/config.sh.local
popd

screen -d -m /root/abrt/tests/runtests/run | tee -a /tmp/abrt-nightly-test.log
