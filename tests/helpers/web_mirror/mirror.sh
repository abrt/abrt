#!/bin/bash
source='http://localhost/'
remote='rmarko@fedorapeople.org:~/public_html/'
target="$HOME/web_mirror/data"
cmd="wget -k -E -r -l 20 -p -nH -P $target"

rm -rf "$target"
mkdir "$target"

$cmd $source

cd $target
pushd abrt

for platform in $( find . -maxdepth 1 -mindepth 1 -type d ); do
    pushd $platform
    for rdir in $( find . -maxdepth 1 -mindepth 1 -type d ); do
        pushd $rdir
            sed -i 's/index.html#/#/' index.html
        popd
    done
    popd
done

popd
ssh_cmd='ssh -i /home/virt_manage/web_mirror/fp_rsa -o StrictHostKeyChecking=no'
rsync -rvz -e "$ssh_cmd" * $remote
cd -
rm -rf "$target"
