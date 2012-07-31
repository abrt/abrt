#! /bin/bash

pushd $PWD && {
    cd /root && {
        git clone git://abrt.brq.redhat.com/abrt.git ./abrt && {
            cd ./abrt && {
                case $(cut -d: -f4,5 /etc/system-release-cpe) in
                    enterprise_linux:6*)
                        git checkout rhel6
                        ;;
                    *)
                        ;;
                esac
            }
        }
    }
    popd
}

cat > /root/abrt/tests/runtests/aux/config.sh.local << "_END_"
export FORMAT_SCRIPT='aux/scp_format.sh'
export REPORT_SCRIPT='aux/scp_report.sh'

export SCPTO='virt_manage@abrt.brq.redhat.com:~/targets/el6/data/'

export SHUTDOWN=1
_END_


EXCLUDE_TESTS="abrt-nightly-build|abrt-make-check|btparser-make-check|libreport-make-check"
TEST_ORDER="/root/abrt/tests/runtests/aux/test_order"

egrep -v "$EXCLUDE_TESTS" "$TEST_ORDER" > /tmp/test_order && mv /tmp/test_order "$TEST_ORDER"
sed -i 's/OpenGPGCheck.*=.*yes/OpenGPGCheck = no/' /etc/abrt/abrt-action-save-package-data.conf
screen -d -m /root/abrt/tests/runtests/run | tee -a /tmp/abrt-nightly-test.log